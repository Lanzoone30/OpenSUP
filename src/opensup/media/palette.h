#pragma once

#include <cstdint>
#include <array>
#include <map>
#include <vector>
#include <optional>
#include <string_view>

namespace opensup {
namespace media {

struct palette_entry_t {
    uint8_t y     = 16;
    uint8_t cr    = 128;
    uint8_t cb    = 128;
    uint8_t alpha = 0;

    palette_entry_t() = default;
    palette_entry_t(uint8_t y_, uint8_t cr_, uint8_t cb_, uint8_t alpha_)
        : y(y_), cr(cr_), cb(cb_), alpha(alpha_) {}

    [[nodiscard]] palette_entry_t to_rgba(std::string_view matrix = "bt709") const;
    static palette_entry_t from_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                                      std::string_view matrix = "bt709");

    [[nodiscard]] std::array<uint8_t, 4> to_bytes() const noexcept;
    static palette_entry_t from_bytes(const uint8_t* data);

    bool operator==(const palette_entry_t& other) const noexcept;
    bool operator!=(const palette_entry_t& other) const noexcept { return !(*this == other); }
};

class palette_t {
public:
    palette_t() = default;
    explicit palette_t(const std::map<uint8_t, palette_entry_t>& entries);

    void set(uint8_t index, const palette_entry_t& entry);
    std::optional<palette_entry_t> get(uint8_t index) const;
    bool has(uint8_t index) const noexcept;

    void remove(uint8_t index);
    void clear() noexcept { m_entries.clear(); }

    void offset(int delta);

    [[nodiscard]] size_t size() const noexcept { return m_entries.size(); }
    [[nodiscard]] bool empty() const noexcept { return m_entries.empty(); }

    [[nodiscard]] palette_t diff(const palette_t& other) const;

    [[nodiscard]] std::vector<uint8_t> to_bytes() const;
    static palette_t from_bytes(const uint8_t* data, size_t length);

    [[nodiscard]] const std::map<uint8_t, palette_entry_t>& entries() const noexcept { return m_entries; }

    bool operator==(const palette_t& other) const noexcept;
    bool operator!=(const palette_t& other) const noexcept { return !(*this == other); }

private:
    std::map<uint8_t, palette_entry_t> m_entries;
};

// YCbCr clipping helpers
[[nodiscard]] uint8_t clip_y(uint32_t v) noexcept;
[[nodiscard]] uint8_t clip_cbcr(uint32_t v) noexcept;

} // namespace media
} // namespace opensup
