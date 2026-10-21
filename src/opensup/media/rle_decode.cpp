// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Adapted from SUPer by cubicibo (https://github.com/cubicibo/SUPer), GPL-3.0-or-later.

#include "opensup/pch.h"
#include "opensup/media/rle_decode.h"

#include <functional>

namespace opensup {
namespace media {

namespace {

/// One decoded run: a repeat count and its palette index.
struct run_t {
    uint32_t count;
    uint8_t color;
};

/// Walk the RLE stream, emitting runs in order. Mirrors the encode_rle command
/// layout: 0x00 introduces a length (bit6 = 14-bit length, bit7 = non-zero run
/// with the colour in the next byte); 0x00 0x00 is end-of-line; any other byte
/// is a raw colour repeated twice when the following byte is identical.
/// @return false on a truncated run or more pixels than @p max_pixels.
bool walk(const std::vector<uint8_t>& data, uint32_t max_pixels,
          const std::function<void(const run_t&)>& emit)
{
    size_t i = 0;
    uint32_t produced = 0;

    while (i < data.size()) {
        const uint8_t b = data[i++];
        uint32_t len = 1;
        uint8_t color = b;
        if (b == 0x00) {
            if (i >= data.size()) return false;
            const uint8_t c = data[i++];
            if (c == 0x00) continue;  // end of line
            len = (c & 0x40) ? 0u : static_cast<uint32_t>(c & 0x3F);
            if (c & 0x40) {
                if (i >= data.size()) return false;
                len = (static_cast<uint32_t>(c & 0x3F) << 8) | data[i++];
            }
            if (len == 0) return false;
            color = 0;
            if (c & 0x80) {
                if (i >= data.size()) return false;
                color = data[i++];
            }
        } else if (i < data.size() && data[i] == b) {
            ++i;  // raw colour twice
            len = 2;
        }
        if (produced + len > max_pixels) return false;
        emit({len, color});
        produced += len;
    }
    return true;
}

}  // namespace

std::vector<uint8_t> decode_rle_checked(const std::vector<uint8_t>& data,
                                        int width, int height, bool& ok)
{
    ok = false;
    if (width <= 0 || height <= 0) return {};

    const auto w = static_cast<uint32_t>(width);
    const uint32_t max_pixels = w * static_cast<uint32_t>(height);
    std::vector<uint8_t> out;
    out.reserve(max_pixels);

    uint32_t line_px = 0;
    bool lines_ok = true;
    const bool walked = walk(data, max_pixels, [&](const run_t& r) {
        if (line_px + r.count > w) lines_ok = false;  // overlong line
        out.insert(out.end(), r.count, r.color);
        line_px += r.count;
        if (line_px == w) line_px = 0;  // line boundary reached
    });

    if (!walked) return {};
    if (line_px != 0) lines_ok = false;  // trailing partial line

    const size_t expected = static_cast<size_t>(width) * static_cast<size_t>(height);
    if (out.size() < expected) {
        out.resize(expected, 0);  // pad short streams for census use
        lines_ok = false;
    } else if (out.size() > expected) {
        out.resize(expected);
        lines_ok = false;
    }
    ok = lines_ok;
    return out;
}

std::vector<uint8_t> decode_rle(const std::vector<uint8_t>& data, int width, int height)
{
    bool ok = false;
    return decode_rle_checked(data, width, height, ok);
}

rle_stats_t rle_stats(const std::vector<uint8_t>& data, int width, int height)
{
    rle_stats_t st;
    bool ok = false;
    const auto px = decode_rle_checked(data, width, height, ok);
    if (px.empty()) return st;
    st.pixels = px.size();
    for (uint8_t v : px)
        if (v != 0) ++st.non_empty;
    return st;
}

}
}
