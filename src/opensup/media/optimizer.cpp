#include "opensup/pch.h"
#include "opensup/media/optimizer.h"
#include "opensup/common/logger.h"

#include <cmath>
#include <algorithm>
#include <cstring>
#include <map>
#include <set>

#include <libimagequant.h>

namespace opensup {
namespace media {

using common::logger_c;

// ── libimagequant backend ──
quantize_result_t
libimagequant_t::quantize(const std::vector<uint8_t>& rgba,
                           int width, int height, int max_colors)
{
    quantize_result_t result;

    liq_attr* handle = liq_attr_create();
    if (!handle) {
        logger_c::instance().error("Failed to create libimagequant handle");
        return result;
    }

    liq_set_max_colors(handle, max_colors);
    liq_set_quality(handle, 0, 100);

    liq_image* img = liq_image_create_rgba(handle, rgba.data(), width, height, 0);
    if (!img) {
        liq_attr_destroy(handle);
        logger_c::instance().error("Failed to create libimagequant image");
        return result;
    }

    liq_result* quant_result = nullptr;
    if (liq_image_quantize(img, handle, &quant_result) != LIQ_OK) {
        liq_image_destroy(img);
        liq_attr_destroy(handle);
        logger_c::instance().error("libimagequant quantization failed");
        return result;
    }

    // Get palette
    const liq_palette* pal = liq_get_palette(quant_result);
    for (int i = 0; i < static_cast<int>(pal->count); i++) {
        auto& e = pal->entries[i];
        auto entry = palette_entry_t::from_rgba(e.r, e.g, e.b, e.a);
        result.palette.set(static_cast<uint8_t>(i), entry);
    }
    auto pal_count = pal->count;  // save before destroy (use-after-free on MinGW)

    // Get indexed pixels
    auto w = static_cast<size_t>(width);
    auto h = static_cast<size_t>(height);
    std::vector<uint8_t> indexed(w * h);
    if (liq_write_remapped_image(quant_result, img, indexed.data(),
                                  w * h) != LIQ_OK) {
        logger_c::instance().error("libimagequant remap failed");
    }

    result.indexed = std::move(indexed);

    liq_result_destroy(quant_result);
    liq_image_destroy(img);
    liq_attr_destroy(handle);

    logger_c::instance().log(common::log_level_e::hdebug, "libimagequant: " +
                                std::to_string(pal_count) + " colors");
    return result;
}

// ── HexTree backend (stdlib-based fallback) ──
// ponytail: simple octree-based color quantizer, O(n) per pixel.
// Replace with full brule HexTree port if quality matters.

struct octree_node_t {
    int r_sum = 0, g_sum = 0, b_sum = 0, a_sum = 0;
    int pixel_count = 0;
    int level = 0;
    octree_node_t* children[8] = {};
    bool leaf = true;

    ~octree_node_t() {
        for (auto* c : children) delete c;
    }
};

static void
octree_add(octree_node_t* node, uint8_t r, uint8_t g, uint8_t b, uint8_t a, int level)
{
    if (level > 7) {
        node->r_sum += r;
        node->g_sum += g;
        node->b_sum += b;
        node->a_sum += a;
        node->pixel_count++;
        return;
    }
    int idx = ((r >> (7 - level)) & 1) << 2 |
              ((g >> (7 - level)) & 1) << 1 |
              ((b >> (7 - level)) & 1);
    if (!node->children[idx]) {
        node->children[idx] = new octree_node_t();
        node->children[idx]->level = level + 1;
        node->leaf = false;
    }
    octree_add(node->children[idx], r, g, b, a, level + 1);
}

static int
octree_collect_leaves(octree_node_t* node,
                       std::vector<std::pair<uint8_t, palette_entry_t>>& palette_out)
{
    if (node->leaf) {
        if (node->pixel_count > 0) {
            auto idx = static_cast<uint8_t>(palette_out.size());
            palette_out.emplace_back(idx, palette_entry_t::from_rgba(
                static_cast<uint8_t>(node->r_sum / node->pixel_count),
                static_cast<uint8_t>(node->g_sum / node->pixel_count),
                static_cast<uint8_t>(node->b_sum / node->pixel_count),
                static_cast<uint8_t>(node->a_sum / node->pixel_count)));
        }
        return 1;
    }
    int count = 0;
    for (int i = 0; i < 8; i++) {
        if (node->children[i])
            count += octree_collect_leaves(node->children[i], palette_out);
    }
    return count;
}

// ponytail: octree pixel mapping done in hextree_t::quantize via nearest-color search


quantize_result_t
hextree_t::quantize(const std::vector<uint8_t>& rgba,
                     int width, int height, int /*max_colors*/)
{
    quantize_result_t result;
    octree_node_t root;

    auto w = static_cast<size_t>(width);
    auto h = static_cast<size_t>(height);
    size_t total = w * h;
    for (size_t i = 0; i < total; i++) {
        octree_add(&root, rgba[i * 4], rgba[i * 4 + 1],
                    rgba[i * 4 + 2], rgba[i * 4 + 3], 0);
    }

    std::vector<std::pair<uint8_t, palette_entry_t>> pal_entries;
    int n_colors = octree_collect_leaves(&root, pal_entries);

    for (auto& [idx, entry] : pal_entries) {
        result.palette.set(idx, entry);
    }

    // Map pixels to nearest palette entry
    std::vector<uint8_t> indexed(total, 0);
    auto pal_rgba = result.palette.to_bytes();

    for (size_t i = 0; i < total; i++) {
        uint8_t r = rgba[i * 4], g = rgba[i * 4 + 1];
        uint8_t b = rgba[i * 4 + 2], a = rgba[i * 4 + 3];
        uint8_t best_idx = 0;
        int best_dist = INT32_MAX;

        for (auto& [idx, entry] : pal_entries) {
            auto e_rgba = entry.to_rgba();
            // to_rgba returns {y=B, cr=G, cb=R, alpha=A}
            int dr = static_cast<int>(r) - static_cast<int>(e_rgba.cb);
            int dg = static_cast<int>(g) - static_cast<int>(e_rgba.cr);
            int db = static_cast<int>(b) - static_cast<int>(e_rgba.y);
            int da = static_cast<int>(a) - static_cast<int>(e_rgba.alpha);
            int dist = dr * dr + dg * dg + db * db + da * da;
            if (dist < best_dist) {
                best_dist = dist;
                best_idx = idx;
            }
        }
        indexed[i] = best_idx;
    }

    result.indexed = std::move(indexed);
    logger_c::instance().log(common::log_level_e::hdebug, "HexTree: " +
                                std::to_string(n_colors) + " colors");
    return result;
}

std::vector<std::unique_ptr<quantizer_base_c>>
optimiser_c::get_available()
{
    std::vector<std::unique_ptr<quantizer_base_c>> backends;
    backends.push_back(std::make_unique<libimagequant_t>());
    backends.push_back(std::make_unique<hextree_t>());
    return backends;
}

} // namespace media
} // namespace opensup
