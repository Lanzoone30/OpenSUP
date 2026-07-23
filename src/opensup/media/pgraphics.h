#pragma once

#include <cstdint>
#include <vector>
#include <optional>
#include <memory>

#include "opensup/media/palette.h"
#include "opensup/common/geometry.h"

namespace opensup { namespace core {
class pds_c;
}} // forward decls

namespace opensup {
namespace media {

// ── RLE Codec ──
std::vector<uint8_t> encode_rle(const std::vector<uint8_t>& bitmap, int width, int height);
std::vector<uint8_t> decode_rle(const std::vector<uint8_t>& data, int width, int height);
std::vector<uint8_t> bitmap_to_rgba(const std::vector<uint8_t>& indexed,
                                     const palette_t& palette, int width, int height);

// ── PGDecoder Timing Constants ──
struct pg_decoder_t {
    static constexpr double RX = 2e6;
    static constexpr double RD = 16e6;
    static constexpr double RC = 32e6;
    static constexpr double FREQ = 90000.0;
    static constexpr size_t DECODED_BUF_SIZE = 4 * 1024 * 1024;
    static constexpr size_t CODED_BUF_SIZE   = 1 * 1024 * 1024;

    static double decode_obj_duration(int area);
    static double copy_gp_duration(int area);
};

// ── Prospective Object ──
struct prospective_object_t {
    int first_frame;
    std::vector<bool> mask;
    std::vector<common::box_t> boxes;
    common::box_t box;
    int ext_range;

    prospective_object_t(int f, std::vector<bool> m,
                          std::vector<common::box_t> b, common::box_t bx);

    [[nodiscard]] bool is_active(int frame) const noexcept;
    [[nodiscard]] bool is_visible(int frame) const noexcept;
    [[nodiscard]] std::optional<common::box_t> get_bbox_at(int frame) const noexcept;
    bool is_visible_extended(int frame) const noexcept;
    void set_extended_visibility_limit(int f_max) noexcept;
};

// ── Buffer Slot ──
struct buffer_slot_t {
    int width  = 0;
    int height = 0;
    double pts = -1.0;
    int version = -1;

    buffer_slot_t() = default;
    buffer_slot_t(int w, int h);

    [[nodiscard]] int size() const noexcept { return width * height; }
    [[nodiscard]] uint8_t version_as_byte() const noexcept;
    [[nodiscard]] bool writable_at(double dts) const noexcept { return pts <= dts; }
    void lock_until(double new_pts) noexcept;
};

// ── PG Object Buffer ──
class pg_object_buffer_t {
public:
    static constexpr int MAX_OBJECTS = 64;

    explicit pg_object_buffer_t(size_t max_size = pg_decoder_t::DECODED_BUF_SIZE,
                                 size_t margin = 0);

    [[nodiscard]] size_t get_free_size() const noexcept;
    void reset() noexcept;

    std::pair<std::optional<int>, buffer_slot_t*> request_slot(int width, int height, double dts);
    buffer_slot_t* get(int slot_id) noexcept;
    std::optional<int> get_slot_version(int slot_id) const noexcept;
    bool allocate_id(int slot_id, int width, int height) noexcept;
    std::optional<int> allocate(int width, int height) noexcept;

private:
    size_t m_max_size;
    std::unordered_map<int, buffer_slot_t> m_slots;
    std::optional<int> find_free_id() const noexcept;
};

// ── PG Palette (versioned palette) ──
class pg_palette_c : public palette_t {
public:
    int version = 0;
    double pts = -1.0;

    void lock_until(double new_pts) noexcept;
    [[nodiscard]] bool writable_at(double dts) const noexcept;
    [[nodiscard]] uint8_t version_as_byte() const noexcept;
    void store(const palette_t& pal, double new_pts);
};

// ── Palette Manager ──
class palette_manager_t {
public:
    static constexpr int N_PALETTES = 8;

    palette_manager_t();

    std::optional<int> get_palette(double dts);
    int get_palette_version(int palette_id);
    bool lock_palette(int palette_id, double pts, double dts, bool force = false);
    std::vector<core::pds_c> assign_palette(int palette_id, const palette_t& palette,
                                              double pts, double dts);

private:
    std::array<pg_palette_c, N_PALETTES> m_palettes;
};

} // namespace media
} // namespace opensup
