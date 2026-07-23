#include "opensup/pch.h"
#include "opensup/media/pgraphics.h"
#include "opensup/core/segments.h"

#include <cmath>
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace opensup {
namespace media {

// ── RLE Codec ──
std::vector<uint8_t>
encode_rle(const std::vector<uint8_t>& bitmap, int width, int height)
{
    // PGS RLE encoder matching brule library format (US 7912305 B1)
    // Command byte: bit7=1 → non-zero color follows, bit7=0 → zero fill
    // bit6=1 → extended 14-bit length, 0 → 6-bit length (1-63)
    // bits5-0: length (direct, no offset)
    // EOL: 0x00 0x00 at end of each scanline
    std::vector<uint8_t> out;

    for (int y = 0; y < height; y++) {
        int line_end = (y + 1) * width;
        int pos = y * width;

        while (pos < line_end) {
            uint8_t pixel = bitmap[static_cast<size_t>(pos)];
            int run_start = pos;
            while (pos < line_end && bitmap[static_cast<size_t>(pos)] == pixel)
                pos++;
            int run_len = pos - run_start;

            if (pixel == 0) {
                // Zero run
                while (run_len > 0) {
                    int chunk = (run_len > 16383) ? 16383 : run_len;
                    if (chunk < 64) {
                        out.push_back(0x00);
                        out.push_back(static_cast<uint8_t>(chunk));
                    } else {
                        out.push_back(0x00);
                        out.push_back(static_cast<uint8_t>(0x40 | (chunk >> 8)));
                        out.push_back(static_cast<uint8_t>(chunk & 0xFF));
                    }
                    run_len -= chunk;
                }
            } else {
                // Non-zero run
                while (run_len > 0) {
                    if (run_len <= 2) {
                        out.push_back(pixel);
                        out.push_back(pixel);
                        if (run_len == 1) {
                            // We wrote one extra, need to remove it
                            out.pop_back();
                        }
                        run_len = 0;
                    } else {
                        int chunk = (run_len > 16383) ? 16383 : run_len;
                        if (chunk < 64) {
                            out.push_back(0x00);
                            out.push_back(static_cast<uint8_t>(0x80 | chunk));
                            out.push_back(pixel);
                        } else {
                            out.push_back(0x00);
                            out.push_back(static_cast<uint8_t>(0xC0 | (chunk >> 8)));
                            out.push_back(static_cast<uint8_t>(chunk & 0xFF));
                            out.push_back(pixel);
                        }
                        run_len -= chunk;
                    }
                }
            }
        }
        // EOL marker: 0x00 0x00
        out.push_back(0x00);
        out.push_back(0x00);
    }

    return out;
}

std::vector<uint8_t>
decode_rle(const std::vector<uint8_t>& data, int width, int height)
{
    // PGS RLE decoder matching brule format
    // width and height are used only to know total_pixels
    std::vector<uint8_t> out;
    auto total_pixels = static_cast<size_t>(width) * static_cast<size_t>(height);
    out.reserve(total_pixels);

    size_t pos = 0;
    while (pos < data.size() && out.size() < total_pixels) {
        uint8_t b = data[pos++];

        if (b == 0x00) {
            if (pos >= data.size()) break;
            uint8_t cmd = data[pos++];

            if (cmd == 0x00) {
                // EOL marker — continue to next scanline
                continue;
            }

            // Extract count: direct value
            int count;
            if (cmd & 0x40) {
                // Extended 14-bit length
                if (pos >= data.size()) break;
                uint8_t lo = data[pos++];
                count = ((cmd & 0x3F) << 8) | lo;
            } else {
                // Short 6-bit length
                count = cmd & 0x3F;
            }

            if (cmd & 0x80) {
                // Non-zero run
                if (pos >= data.size()) break;
                uint8_t pixel = data[pos++];
                for (int i = 0; i < count; i++)
                    out.push_back(pixel);
            } else {
                // Zero run
                for (int i = 0; i < count; i++)
                    out.push_back(0);
            }
        } else {
            // Single non-zero pixel (or start of inline run of 2)
            if (pos < data.size() && data[pos] == b && out.size() < total_pixels) {
                // Inline run of 2
                pos++;
                out.push_back(b);
                out.push_back(b);
            } else {
                out.push_back(b);
            }
        }
    }
    return out;
}

std::vector<uint8_t>
bitmap_to_rgba(const std::vector<uint8_t>& indexed,
                const palette_t& palette, int width, int height)
{
    size_t total = static_cast<size_t>(width) * static_cast<size_t>(height);
    std::vector<uint8_t> rgba(total * 4, 0);

    for (size_t i = 0; i < total; i++) {
        auto entry = palette.get(indexed[i]);
        auto rgb = entry.value_or(palette_entry_t{}).to_rgba();
        rgba[i * 4 + 0] = rgb.cb;    // R (to_rgba stores BGR in {y,cr,cb,alpha})
        rgba[i * 4 + 1] = rgb.cr;    // G
        rgba[i * 4 + 2] = rgb.y;     // B
        rgba[i * 4 + 3] = rgb.alpha; // A
    }
    return rgba;
}

// ── PGDecoder ──
double pg_decoder_t::decode_obj_duration(int area)
{
    return std::ceil(FREQ * static_cast<double>(area) / RD) / FREQ;
}

double pg_decoder_t::copy_gp_duration(int area)
{
    return std::ceil(FREQ * static_cast<double>(area) / RC) / FREQ;
}

// ── Prospective Object ──
prospective_object_t::prospective_object_t(int f, std::vector<bool> m,
                                             std::vector<common::box_t> b,
                                             common::box_t bx)
    : first_frame(f), mask(std::move(m)), boxes(std::move(b)), box(bx)
    , ext_range(first_frame + static_cast<int>(mask.size()))
{
    if (mask.size() != boxes.size())
        throw std::invalid_argument("mask and boxes length mismatch");
    if (box.area() <= 0)
        throw std::invalid_argument("box area must be > 0");
}

bool prospective_object_t::is_active(int frame) const noexcept
{
    return frame >= first_frame &&
           frame < first_frame + static_cast<int>(mask.size());
}

bool prospective_object_t::is_visible(int frame) const noexcept
{
    if (is_active(frame))
        return mask[static_cast<size_t>(frame - first_frame)];
    return false;
}

std::optional<common::box_t>
prospective_object_t::get_bbox_at(int frame) const noexcept
{
    if (is_active(frame))
        return boxes[static_cast<size_t>(frame - first_frame)];
    return std::nullopt;
}

bool prospective_object_t::is_visible_extended(int frame) const noexcept
{
    return frame > first_frame && !is_active(frame) && frame < ext_range;
}

void prospective_object_t::set_extended_visibility_limit(int f_max) noexcept
{
    ext_range = f_max;
}

// ── Buffer Slot ──
buffer_slot_t::buffer_slot_t(int w, int h)
    : width(w), height(h), pts(-1.0), version(-1)
{
    if (w < 8 || w > 4096 || h < 8 || h > 4096)
        throw std::invalid_argument("Illegal PG object dimensions");
}

uint8_t buffer_slot_t::version_as_byte() const noexcept
{
    return static_cast<uint8_t>(version & 0xFF);
}

void buffer_slot_t::lock_until(double new_pts) noexcept
{
    pts = new_pts;
    version++;
}

// ── PG Object Buffer ──
pg_object_buffer_t::pg_object_buffer_t(size_t max_size, size_t margin)
    : m_max_size(max_size > margin ? max_size - margin : 0) {}

size_t pg_object_buffer_t::get_free_size() const noexcept
{
    size_t used = 0;
    for (auto& [id, slot] : m_slots)
        used += static_cast<size_t>(slot.size());
    return (m_max_size > used) ? (m_max_size - used) : 0;
}

void pg_object_buffer_t::reset() noexcept
{
    m_slots.clear();
}

std::optional<int>
pg_object_buffer_t::find_free_id() const noexcept
{
    for (int k = 0; k < MAX_OBJECTS; k++) {
        if (m_slots.find(k) == m_slots.end())
            return k;
    }
    return std::nullopt;
}

std::pair<std::optional<int>, buffer_slot_t*>
pg_object_buffer_t::request_slot(int width, int height, double dts)
{
    // Try to reuse existing slot with matching dimensions
    for (auto& [id, slot] : m_slots) {
        if (slot.width == width && slot.height == height && slot.writable_at(dts))
            return {id, &slot};
    }

    auto id = find_free_id();
    if (id.has_value()) {
        buffer_slot_t bs(width, height);
        if (get_free_size() >= static_cast<size_t>(bs.size())) {
            auto [it, inserted] = m_slots.emplace(*id, std::move(bs));
            if (inserted)
                return {*id, &it->second};
        }
    }
    return {std::nullopt, nullptr};
}

buffer_slot_t* pg_object_buffer_t::get(int slot_id) noexcept
{
    auto it = m_slots.find(slot_id);
    return (it != m_slots.end()) ? &it->second : nullptr;
}

std::optional<int>
pg_object_buffer_t::get_slot_version(int slot_id) const noexcept
{
    auto it = m_slots.find(slot_id);
    if (it != m_slots.end())
        return it->second.version_as_byte();
    return std::nullopt;
}

bool pg_object_buffer_t::allocate_id(int slot_id, int width, int height) noexcept
{
    if (slot_id < 0 || slot_id >= MAX_OBJECTS) return false;
    if (m_slots.find(slot_id) != m_slots.end()) return false;

    buffer_slot_t bs(width, height);
    if (get_free_size() < static_cast<size_t>(bs.size())) return false;

    m_slots.emplace(slot_id, std::move(bs));
    return true;
}

std::optional<int>
pg_object_buffer_t::allocate(int width, int height) noexcept
{
    auto id = find_free_id();
    if (!id.has_value()) return std::nullopt;

    buffer_slot_t bs(width, height);
    if (get_free_size() < static_cast<size_t>(bs.size())) return std::nullopt;

    m_slots.emplace(*id, std::move(bs));
    return id;
}

// ── PG Palette ──
void pg_palette_c::lock_until(double new_pts) noexcept
{
    pts = new_pts;
    version++;
}

bool pg_palette_c::writable_at(double dts) const noexcept
{
    return pts < dts;
}

uint8_t pg_palette_c::version_as_byte() const noexcept
{
    return static_cast<uint8_t>((version - 1) & 0xFF);
}

void pg_palette_c::store(const palette_t& pal, double new_pts)
{
    for (auto& [k, v] : pal.entries())
        set(k, v);
    version++;
    pts = new_pts;
}

// ── Palette Manager ──
palette_manager_t::palette_manager_t() {}

std::optional<int>
palette_manager_t::get_palette(double dts)
{
    // ponytail: linear scan of 8 entries, trivial
    auto* best = static_cast<decltype(m_palettes.data())>(nullptr);
    int best_v = INT32_MAX;

    for (auto& p : m_palettes) {
        if (p.writable_at(dts)) {
            int v = p.version >> 8;
            if (!best || v < best_v) {
                best = &p;
                best_v = v;
            }
        }
    }
    if (!best) return std::nullopt;
    return static_cast<int>(best - m_palettes.data());
}

int palette_manager_t::get_palette_version(int palette_id)
{
    if (palette_id < 0 || palette_id >= N_PALETTES)
        throw std::invalid_argument("Invalid palette ID");
    return m_palettes[static_cast<size_t>(palette_id)].version_as_byte();
}

bool palette_manager_t::lock_palette(int palette_id, double pts_, double dts, bool force)
{
    if (palette_id < 0 || palette_id >= N_PALETTES) return false;
    auto& p = m_palettes[static_cast<size_t>(palette_id)];
    if (p.writable_at(dts) || force) {
        p.lock_until(pts_);
        return true;
    }
    return false;
}

std::vector<core::pds_c>
palette_manager_t::assign_palette(int palette_id, const palette_t& palette,
                                    double pts_, double dts)
{
    if (palette_id < 0 || palette_id >= N_PALETTES)
        throw std::invalid_argument("Invalid palette ID");

    auto& pgpal = m_palettes[static_cast<size_t>(palette_id)];
    std::vector<core::pds_c> result;

    if (palette.empty()) {
        auto pds = core::pds_c::from_scratch(pgpal, pgpal.version_as_byte(),
                                               static_cast<uint8_t>(palette_id),
                                               pts_, dts);
        result.push_back(std::move(pds));
    } else {
        pgpal.store(palette, pts_);
        auto pds = core::pds_c::from_scratch(palette, pgpal.version_as_byte(),
                                               static_cast<uint8_t>(palette_id),
                                               pts_, dts);
        result.push_back(std::move(pds));
    }
    return result;
}

} // namespace media
} // namespace opensup
