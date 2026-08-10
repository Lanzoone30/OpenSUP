// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Adapted from SUPer by cubicibo (https://github.com/cubicibo/SUPer), GPL-3.0-or-later.

#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <optional>
#include <variant>

#include "opensup/media/palette.h"

namespace opensup {
namespace core {

constexpr double PGS_FREQ = 90000.0;
constexpr size_t PGS_HEADER_LEN = 13;
constexpr uint8_t PGS_MAGIC[2] = {'P', 'G'};

// PGS segment types per Blu-ray spec: PDS=0x14, ODS=0x15, PCS=0x16, WDS=0x17, ENDS=0x80
// Verified against SUPer-main (segments.py:43) and PGSEncoder-master (PGSStructures.h:25-29)
enum class segment_type_e : uint8_t {
    pds  = 0x14,
    ods  = 0x15,
    pcs  = 0x16,
    wds  = 0x17,
    end  = 0x80,
};

// ── Window Definition ──
struct window_definition_t {
    uint8_t window_id = 0;
    uint16_t h_pos = 0;
    uint16_t v_pos = 0;
    uint16_t width  = 0;
    uint16_t height = 0;

    [[nodiscard]] std::vector<uint8_t> to_bytes() const;
    static window_definition_t from_bytes(const uint8_t* data);
};

// ── Composition Object (CObject) ──
struct c_object_t {
    enum flags_e : uint8_t {
        cropped  = 0x80,
        forced   = 0x40,
        standard = 0x00,
    };

    uint16_t o_id      = 0;
    uint8_t  window_id = 0;
    uint8_t  flags     = 0;
    uint16_t h_pos     = 0;
    uint16_t v_pos     = 0;
    // Cropping fields (only valid if cropped flag is set)
    uint16_t hc_pos    = 0;
    uint16_t vc_pos    = 0;
    uint16_t c_w       = 0;
    uint16_t c_h       = 0;

    [[nodiscard]] bool is_cropped() const noexcept { return flags & cropped; }
    [[nodiscard]] bool is_forced() const noexcept  { return flags & forced; }

    void set_forced(bool f) noexcept;

    [[nodiscard]] std::vector<uint8_t> to_bytes() const;
    static c_object_t from_bytes(const uint8_t* data, bool cropped);
};

// ── Base PGS Segment ──
/**
 * @brief A single PGS segment (header + payload) from a subtitle stream.
 * Subclasses specialize per segment type; to_bytes() re-serializes with the
 * current field values.
 */
class pg_segment_c {
public:
    pg_segment_c() = default;
    explicit pg_segment_c(const std::vector<uint8_t>& data);

    /// Presentation timestamp in seconds.
    [[nodiscard]] double pts() const noexcept;
    void set_pts(double pts) noexcept;
    /// Decode timestamp in seconds.
    [[nodiscard]] double dts() const noexcept;
    void set_dts(double dts) noexcept;

    /// Raw PTS in 90 kHz ticks.
    [[nodiscard]] uint32_t tpts() const noexcept;
    void set_tpts(uint32_t pts) noexcept;
    /// Raw DTS in 90 kHz ticks.
    [[nodiscard]] uint32_t tdts() const noexcept;
    void set_tdts(uint32_t dts) noexcept;

    /// Segment type (PDS/ODS/PCS/WDS/ENDS).
    [[nodiscard]] segment_type_e type() const noexcept;
    /// Payload length in bytes.
    [[nodiscard]] uint16_t size() const noexcept;

    /// Copy of the payload (without header).
    [[nodiscard]] std::vector<uint8_t> payload() const;

    /// Full serialized segment: 13-byte header + payload.
    [[nodiscard]] virtual std::vector<uint8_t> to_bytes() const;
    /// Recompute derived payload fields (calls update_length()).
    virtual void update();
    void update_length();

    virtual ~pg_segment_c() = default;

    static std::vector<uint8_t> make_header(segment_type_e type);

protected:
    std::vector<uint8_t> m_data;
};

// ── PCS: Presentation Composition Segment ──
/**
 * @brief PCS segment: the "scene graph" of a display set.
 * Declares which composition objects are shown, in which windows, and how they
 * relate to the previous display set via composition_state. This state machine
 * is the contract that decides reuse legality.
 */
class pcs_c : public pg_segment_c {
public:
    /// How this composition relates to the previous display set.
    enum class composition_state_e : uint8_t {
        normal          = 0x00,
        acquisition     = 0x40,
        epoch_start     = 0x80,
        epoch_continue  = 0xC0,
    };

    std::vector<c_object_t> cobjects;

    pcs_c() = default;
    explicit pcs_c(const std::vector<uint8_t>& data);

    [[nodiscard]] uint16_t video_width() const noexcept;
    void set_video_width(uint16_t w) noexcept;
    [[nodiscard]] uint16_t video_height() const noexcept;
    void set_video_height(uint16_t h) noexcept;
    [[nodiscard]] uint8_t fps() const noexcept;
    void set_fps(uint8_t f) noexcept;
    [[nodiscard]] uint16_t composition_n() const noexcept;
    void set_composition_n(uint16_t n) noexcept;
    [[nodiscard]] composition_state_e composition_state() const noexcept;
    void set_composition_state(composition_state_e s) noexcept;
    [[nodiscard]] bool pal_flag() const noexcept;
    void set_pal_flag(bool f) noexcept;
    [[nodiscard]] uint8_t pal_id() const noexcept;
    void set_pal_id(uint8_t id) noexcept;
    [[nodiscard]] uint8_t n_objects() const noexcept;

    void update() override;
    [[nodiscard]] std::vector<uint8_t> to_bytes() const override;

    /// Builds a PCS from field values instead of parsing a raw payload.
    static pcs_c from_scratch(uint16_t width, uint16_t height, uint8_t fps,
                               uint16_t comp_n, composition_state_e state,
                               bool pal_flag, uint8_t pal_id,
                               const std::vector<c_object_t>& cobjects,
                               double pts = 0.0, double dts = 0.0);
};

// ── WDS: Window Definition Segment ──
/**
 * @brief WDS segment: defines the rectangular windows on the video plane.
 * Objects (ODS) are composited inside these windows; a window may be reused
 * across display sets to avoid re-sending its content.
 */
class wds_c : public pg_segment_c {
public:
    std::vector<window_definition_t> windows;

    wds_c() = default;
    explicit wds_c(const std::vector<uint8_t>& data);

    [[nodiscard]] uint8_t n_windows() const noexcept;
    void update() override;
    [[nodiscard]] std::vector<uint8_t> to_bytes() const override;

    /// Builds a WDS from window definitions instead of parsing a raw payload.
    static wds_c from_scratch(const std::vector<window_definition_t>& windows,
                               double pts = 0.0, double dts = 0.0);
};

// ── PDS: Palette Definition Segment ──
/**
 * @brief PDS segment: maps palette indices to YCbCr+alpha colors.
 * Palettes are versioned (p_vn); bumping the version marks the palette as
 * changed for downstream decoders.
 */
class pds_c : public pg_segment_c {
public:
    pds_c() = default;
    explicit pds_c(const std::vector<uint8_t>& data);

    [[nodiscard]] uint8_t p_id() const noexcept;
    void set_p_id(uint8_t id) noexcept;
    [[nodiscard]] uint8_t p_vn() const noexcept;
    void set_p_vn(uint8_t vn) noexcept;

    /// Interprets the raw palette entries as a media::palette_t.
    [[nodiscard]] media::palette_t to_palette() const;
    void set_palette(const media::palette_t& pal);

    /// Builds a PDS from a palette, optionally shifting entry indices.
    static pds_c from_scratch(const media::palette_t& palette,
                               uint8_t p_vn, uint8_t p_id = 0,
                               double pts = 0.0, double dts = 0.0,
                               int offset = 0);
};

// ── ODS: Object Definition Segment ──
/**
 * @brief ODS segment: carries the run-length encoded bitmap of an object.
 * A bitmap larger than the PGS payload limit is split into a sequence of ODS
 * fragments (first/middle/last); a single-object display set uses `single`.
 */
class ods_c : public pg_segment_c {
public:
    /// Position of this fragment within a split bitmap.
    enum class sequence_flags_e : uint8_t {
        first  = 0x80,
        last   = 0x40,
        single = first | last,
        middle = 0x00,
    };

    ods_c() = default;
    explicit ods_c(const std::vector<uint8_t>& data);

    [[nodiscard]] uint16_t o_id() const noexcept;
    void set_o_id(uint16_t id) noexcept;
    [[nodiscard]] uint8_t o_vn() const noexcept;
    void set_o_vn(uint8_t vn) noexcept;
    [[nodiscard]] sequence_flags_e seq_flags() const noexcept;
    void set_seq_flags(sequence_flags_e f) noexcept;
    [[nodiscard]] uint32_t rle_len() const;
    void set_rle_len(uint32_t len);
    [[nodiscard]] uint16_t width() const;
    void set_width(uint16_t w);
    [[nodiscard]] uint16_t height() const;
    void set_height(uint16_t h);
    [[nodiscard]] std::vector<uint8_t> data() const;
    void set_data(const std::vector<uint8_t>& d);

    /// Splits an RLE bitmap into ODS fragment(s) for the PGS payload limit.
    static std::vector<ods_c> from_scratch(uint16_t o_id, uint8_t o_vn,
                                             uint16_t width, uint16_t height,
                                             const std::vector<uint8_t>& rle_data,
                                             double pts = 0.0, double dts = 0.0);
};

// ── ENDS: End of Display Set Segment ──
/**
 * @brief ENDS segment: marks the end of a display set.
 * Every display set must close with ENDS; the decoder flushes its buffers
 * when it arrives.
 */
class ends_c : public pg_segment_c {
public:
    ends_c() = default;
    explicit ends_c(const std::vector<uint8_t>& data);
    static ends_c from_scratch(double pts = 0.0, double dts = 0.0);
};

// ── Display Set ──
/**
 * @brief A display set: all segments shown at one composition time.
 * The unit of PGS encoding. The encoder/optimizer work on collections of display
 * sets (epochs); t_in()/t_out() bound how long this composition is displayed.
 */
class display_set_t {
public:
    std::vector<std::shared_ptr<pg_segment_c>> segments;

    display_set_t() = default;
    explicit display_set_t(std::vector<std::shared_ptr<pg_segment_c>> segs);

    /// Re-serialize all segments (e.g. after editing a field).
    void update();

    /// When this composition becomes visible, in seconds.
    [[nodiscard]] double t_in() const noexcept;
    /// When this composition stops being visible, in seconds.
    [[nodiscard]] double t_out() const noexcept;
    /// Presentation timestamp of the PCS segment.
    [[nodiscard]] double pts() const noexcept;
    void set_pts(double new_pts) noexcept;

    /// The mandatory PCS segment (asserted present).
    [[nodiscard]] pcs_c& pcs();
    /// The optional WDS segment (absent for reuse-only display sets).
    [[nodiscard]] std::optional<std::reference_wrapper<wds_c>> wds();
    [[nodiscard]] std::vector<std::shared_ptr<pds_c>> pds() const;
    [[nodiscard]] std::vector<std::shared_ptr<ods_c>> ods() const;
    [[nodiscard]] ends_c& end();

    /// Parses a raw PGS byte stream into a display set.
    static display_set_t from_bytes(const std::vector<uint8_t>& data);
};

// ── Helper functions ──
[[nodiscard]] segment_type_e segment_type_from_byte(uint8_t b);
[[nodiscard]] uint16_t read_u16_be(const uint8_t* data) noexcept;
[[nodiscard]] uint32_t read_u24_be(const uint8_t* data) noexcept;
[[nodiscard]] uint32_t read_u32_be(const uint8_t* data) noexcept;
void write_u16_be(uint8_t* data, uint16_t val) noexcept;
void write_u24_be(uint8_t* data, uint32_t val) noexcept;
void write_u32_be(uint8_t* data, uint32_t val) noexcept;

} // namespace core
} // namespace opensup
