// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Adapted from SUPer by cubicibo (https://github.com/cubicibo/SUPer), GPL-3.0-or-later.

#include "opensup/pch.h"
#include "opensup/media/pgraphics.h"

#include <vector>

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

} // namespace media
} // namespace opensup