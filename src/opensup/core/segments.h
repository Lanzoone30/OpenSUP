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
class pg_segment_c {
public:
    pg_segment_c() = default;
    explicit pg_segment_c(const std::vector<uint8_t>& data);

    [[nodiscard]] double pts() const noexcept;
    void set_pts(double pts) noexcept;
    [[nodiscard]] double dts() const noexcept;
    void set_dts(double dts) noexcept;

    [[nodiscard]] uint32_t tpts() const noexcept;
    void set_tpts(uint32_t pts) noexcept;
    [[nodiscard]] uint32_t tdts() const noexcept;
    void set_tdts(uint32_t dts) noexcept;

    [[nodiscard]] segment_type_e type() const noexcept;
    [[nodiscard]] uint16_t size() const noexcept;

    [[nodiscard]] std::vector<uint8_t> payload() const;

    [[nodiscard]] virtual std::vector<uint8_t> to_bytes() const;
    virtual void update();
    void update_length();

    virtual ~pg_segment_c() = default;

    static std::vector<uint8_t> make_header(segment_type_e type);

protected:
    std::vector<uint8_t> m_data;
};

// ── PCS: Presentation Composition Segment ──
class pcs_c : public pg_segment_c {
public:
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

    static pcs_c from_scratch(uint16_t width, uint16_t height, uint8_t fps,
                               uint16_t comp_n, composition_state_e state,
                               bool pal_flag, uint8_t pal_id,
                               const std::vector<c_object_t>& cobjects,
                               double pts = 0.0, double dts = 0.0);
};

// ── WDS: Window Definition Segment ──
class wds_c : public pg_segment_c {
public:
    std::vector<window_definition_t> windows;

    wds_c() = default;
    explicit wds_c(const std::vector<uint8_t>& data);

    [[nodiscard]] uint8_t n_windows() const noexcept;
    void update() override;
    [[nodiscard]] std::vector<uint8_t> to_bytes() const override;

    static wds_c from_scratch(const std::vector<window_definition_t>& windows,
                               double pts = 0.0, double dts = 0.0);
};

// ── PDS: Palette Definition Segment ──
class pds_c : public pg_segment_c {
public:
    pds_c() = default;
    explicit pds_c(const std::vector<uint8_t>& data);

    [[nodiscard]] uint8_t p_id() const noexcept;
    void set_p_id(uint8_t id) noexcept;
    [[nodiscard]] uint8_t p_vn() const noexcept;
    void set_p_vn(uint8_t vn) noexcept;

    [[nodiscard]] media::palette_t to_palette() const;
    void set_palette(const media::palette_t& pal);

    static pds_c from_scratch(const media::palette_t& palette,
                               uint8_t p_vn, uint8_t p_id = 0,
                               double pts = 0.0, double dts = 0.0,
                               int offset = 0);
};

// ── ODS: Object Definition Segment ──
class ods_c : public pg_segment_c {
public:
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

    static std::vector<ods_c> from_scratch(uint16_t o_id, uint8_t o_vn,
                                             uint16_t width, uint16_t height,
                                             const std::vector<uint8_t>& rle_data,
                                             double pts = 0.0, double dts = 0.0);
};

// ── ENDS: End of Display Set Segment ──
class ends_c : public pg_segment_c {
public:
    ends_c() = default;
    explicit ends_c(const std::vector<uint8_t>& data);
    static ends_c from_scratch(double pts = 0.0, double dts = 0.0);
};

// ── Display Set ──
class display_set_t {
public:
    std::vector<std::shared_ptr<pg_segment_c>> segments;

    display_set_t() = default;
    explicit display_set_t(std::vector<std::shared_ptr<pg_segment_c>> segs);

    void update();

    [[nodiscard]] double t_in() const noexcept;
    [[nodiscard]] double t_out() const noexcept;
    [[nodiscard]] double pts() const noexcept;
    void set_pts(double new_pts) noexcept;

    [[nodiscard]] pcs_c& pcs();
    [[nodiscard]] std::optional<std::reference_wrapper<wds_c>> wds();
    [[nodiscard]] std::vector<std::shared_ptr<pds_c>> pds() const;
    [[nodiscard]] std::vector<std::shared_ptr<ods_c>> ods() const;
    [[nodiscard]] ends_c& end();

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
