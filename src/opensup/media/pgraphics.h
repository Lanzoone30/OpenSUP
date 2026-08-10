// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Subtitle Encoder for Blu-ray
// Licensed under the GNU General Public License v3.0.
// See LICENSE file for details.
//
// Adapted from the design of SUPer by cubicibo
// (https://github.com/cubicibo/SUPer), GPL-3.0-or-later.
// Independently reimplemented in C++17; the original Python
// implementation is available in Referencias/SUPer-main/.

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
/// Encode an indexed bitmap to PGS run-length format.
std::vector<uint8_t> encode_rle(const std::vector<uint8_t>& bitmap, int width, int height);
/// Decode a PGS run-length payload back to an indexed bitmap.
std::vector<uint8_t> decode_rle(const std::vector<uint8_t>& data, int width, int height);
/// Expand an indexed bitmap to RGBA using the given palette (for rendering).
std::vector<uint8_t> bitmap_to_rgba(const std::vector<uint8_t>& indexed,
                                     const palette_t& palette, int width, int height);

// ── PGDecoder Timing Constants ──
/**
 * @brief Timing constants of a nominal PGS decoder.
 *
 * These model the buffering and rendering delays defined by the Blu-ray
 * spec (receive, decode and composition rates). The optimizer uses them to
 * predict whether a display set can be decoded in time.
 */
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
/**
 * @brief An object being considered for reuse across frames.
 *
 * Tracks, per frame, whether the object is present and visible, and how far
 * it may extend into the future (ext_range). The optimizer builds these to
 * find objects that can stay in the decoder buffer instead of being resent.
 */
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
/**
 * @brief One decoded-object slot in the PG object buffer.
 *
 * A slot holds a decoded bitmap while it can be referenced by future
 * display sets; lock_until() prevents overwriting it while in use.
 */
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
/**
 * @brief Bounded buffer of decoded objects, mirroring the decoder.
 *
 * The Blu-ray decoder has a fixed decoded-object buffer (DECODED_BUF_SIZE);
 * the optimizer reserves slots here so the stream never asks the decoder
 * for more memory than the spec allows.
 */
class pg_object_buffer_t {
public:
    static constexpr int MAX_OBJECTS = 64;

    explicit pg_object_buffer_t(size_t max_size = pg_decoder_t::DECODED_BUF_SIZE,
                                 size_t margin = 0);

    [[nodiscard]] size_t get_free_size() const noexcept;
    void reset() noexcept;

    /// Reserve a writable slot at dts, reusing a free one if possible.
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
/**
 * @brief Palette with decoder-side version and lock timing.
 *
 * Decoders cache palettes by (id, version); bumping the version forces a
 * reload. writable_at() tells whether the palette may be replaced at a
 * given decode time.
 */
class pg_palette_c : public palette_t {
public:
    int version = 0;
    double pts = -1.0;

    void lock_until(double new_pts) noexcept;
    [[nodiscard]] bool writable_at(double dts) const noexcept;
    [[nodiscard]] uint8_t version_as_byte() const noexcept;
    /// Replace palette content, bumping version and lock until new_pts.
    void store(const palette_t& pal, double new_pts);
};

// ── Palette Manager ──
/**
 * @brief Manages the pool of decoder palette slots.
 *
 * Assigns a palette slot per display set, reusing a slot whose previous
 * palette is no longer needed (writable_at) and forcing a version bump
 * when contents change.
 */
class palette_manager_t {
public:
    static constexpr int N_PALETTES = 8;

    palette_manager_t();

    /// Pick a palette slot free at dts, if any.
    std::optional<int> get_palette(double dts);
    int get_palette_version(int palette_id);
    /// Reserve the slot from pts to dts; false if it's still locked.
    bool lock_palette(int palette_id, double pts, double dts, bool force = false);
    /// Store a palette and emit the PDS segments that (re)define it.
    std::vector<core::pds_c> assign_palette(int palette_id, const palette_t& palette,
                                              double pts, double dts);

private:
    std::array<pg_palette_c, N_PALETTES> m_palettes;
};

} // namespace media
} // namespace opensup
