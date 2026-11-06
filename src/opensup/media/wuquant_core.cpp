// SPDX-License-Identifier: GPL-3.0-or-later
// Xiaolin Wu variance-minimization quantizer (cq.c v2) — free to distribute.

#include <algorithm>
#include <cstdint>
#include <vector>

#include "opensup/media/wuquant_core.h"
#include "opensup/media/dither.hpp"

namespace opensup {
namespace media {
namespace {

constexpr int kSide = 33;
constexpr int kHistSize = kSide * kSide * kSide;
constexpr int kMaxColors = 256;

constexpr int kRed = 2;
constexpr int kGreen = 1;
constexpr int kBlue = 0;

inline int lutIndex(int r, int g, int b)
{
    return (r << 10) + (r << 6) + r + (g << 5) + g + b;
}

inline int binOf(int v)
{
    return (v >> 3) + 1;
}

struct box_t {
    int r0 = 0, r1 = 0;
    int g0 = 0, g1 = 0;
    int b0 = 0, b1 = 0;
    int vol = 0;
};

struct moments_t {
    std::vector<int64_t> wt, mr, mg, mb, ma;
    std::vector<double> m2;

    moments_t()
        : wt(kHistSize, 0), mr(kHistSize, 0), mg(kHistSize, 0),
          mb(kHistSize, 0), ma(kHistSize, 0), m2(kHistSize, 0.0) {}
};

void buildHistogram(const uint8_t* rgba, size_t len, moments_t& m)
{
    for (size_t i = 0; i < len; ++i) {
        const uint8_t* p = &rgba[i << 2];
        int r = p[0], g = p[1], b = p[2];
        const int a = p[3];
        if (a == 0) r = g = b = 0; // alpha is the vehicle: no RGB when fully transparent
        const int ind = lutIndex(binOf(r), binOf(g), binOf(b));
        m.wt[ind] += 1;
        m.mr[ind] += r;
        m.mg[ind] += g;
        m.mb[ind] += b;
        m.ma[ind] += a;
        m.m2[ind] += static_cast<double>(r * r + g * g + b * b);
    }
}

/// In-place 3-D prefix sums over bins 1..32 (inclusion-exclusion).
void prefixMoments(moments_t& m)
{
    int64_t area_wt[kSide], area_r[kSide], area_g[kSide], area_b[kSide], area_a[kSide];
    double area_m2[kSide];
    for (int r = 1; r < kSide; ++r) {
        std::fill(std::begin(area_wt), std::end(area_wt), 0);
        std::fill(std::begin(area_r), std::end(area_r), 0);
        std::fill(std::begin(area_g), std::end(area_g), 0);
        std::fill(std::begin(area_b), std::end(area_b), 0);
        std::fill(std::begin(area_a), std::end(area_a), 0);
        std::fill(std::begin(area_m2), std::end(area_m2), 0.0);
        for (int g = 1; g < kSide; ++g) {
            int64_t line_wt = 0, line_r = 0, line_g = 0, line_b = 0, line_a = 0;
            double line_m2 = 0.0;
            for (int b = 1; b < kSide; ++b) {
                const int i1 = lutIndex(r, g, b);
                line_wt += m.wt[i1];
                line_r += m.mr[i1];
                line_g += m.mg[i1];
                line_b += m.mb[i1];
                line_a += m.ma[i1];
                line_m2 += m.m2[i1];
                area_wt[b] += line_wt;
                area_r[b] += line_r;
                area_g[b] += line_g;
                area_b[b] += line_b;
                area_a[b] += line_a;
                area_m2[b] += line_m2;
                const int i0 = i1 - 1089; // [r-1][g][b]
                m.wt[i1] = m.wt[i0] + area_wt[b];
                m.mr[i1] = m.mr[i0] + area_r[b];
                m.mg[i1] = m.mg[i0] + area_g[b];
                m.mb[i1] = m.mb[i0] + area_b[b];
                m.ma[i1] = m.ma[i0] + area_a[b];
                m.m2[i1] = m.m2[i0] + area_m2[b];
            }
        }
    }
}

template <typename T>
T volume(const T* mmt, const box_t& box)
{
    return mmt[lutIndex(box.r1, box.g1, box.b1)]
         - mmt[lutIndex(box.r1, box.g1, box.b0)]
         - mmt[lutIndex(box.r1, box.g0, box.b1)]
         + mmt[lutIndex(box.r1, box.g0, box.b0)]
         - mmt[lutIndex(box.r0, box.g1, box.b1)]
         + mmt[lutIndex(box.r0, box.g1, box.b0)]
         + mmt[lutIndex(box.r0, box.g0, box.b1)]
         - mmt[lutIndex(box.r0, box.g0, box.b0)];
}

template <typename T>
T bottom(const T* mmt, int dir, const box_t& box)
{
    switch (dir) {
    case kRed:
        return -mmt[lutIndex(box.r0, box.g1, box.b1)]
             + mmt[lutIndex(box.r0, box.g1, box.b0)]
             + mmt[lutIndex(box.r0, box.g0, box.b1)]
             - mmt[lutIndex(box.r0, box.g0, box.b0)];
    case kGreen:
        return -mmt[lutIndex(box.r1, box.g0, box.b1)]
             + mmt[lutIndex(box.r1, box.g0, box.b0)]
             + mmt[lutIndex(box.r0, box.g0, box.b1)]
             - mmt[lutIndex(box.r0, box.g0, box.b0)];
    default:
        return -mmt[lutIndex(box.r1, box.g1, box.b0)]
             + mmt[lutIndex(box.r1, box.g0, box.b0)]
             + mmt[lutIndex(box.r0, box.g1, box.b0)]
             - mmt[lutIndex(box.r0, box.g0, box.b0)];
    }
}

template <typename T>
T top(const T* mmt, int dir, int pos, const box_t& box)
{
    switch (dir) {
    case kRed:
        return mmt[lutIndex(pos, box.g1, box.b1)]
             - mmt[lutIndex(pos, box.g1, box.b0)]
             - mmt[lutIndex(pos, box.g0, box.b1)]
             + mmt[lutIndex(pos, box.g0, box.b0)];
    case kGreen:
        return mmt[lutIndex(box.r1, pos, box.b1)]
             - mmt[lutIndex(box.r1, pos, box.b0)]
             - mmt[lutIndex(box.r0, pos, box.b1)]
             + mmt[lutIndex(box.r0, pos, box.b0)];
    default:
        return mmt[lutIndex(box.r1, box.g1, pos)]
             - mmt[lutIndex(box.r1, box.g0, pos)]
             - mmt[lutIndex(box.r0, box.g1, pos)]
             + mmt[lutIndex(box.r0, box.g0, pos)];
    }
}

double variance(const moments_t& m, const box_t& box)
{
    const double dr = static_cast<double>(volume(m.mr.data(), box));
    const double dg = static_cast<double>(volume(m.mg.data(), box));
    const double db = static_cast<double>(volume(m.mb.data(), box));
    const double xx = volume(m.m2.data(), box);
    return xx - (dr * dr + dg * dg + db * db) / static_cast<double>(volume(m.wt.data(), box));
}

/// Best split of `box` along one axis: maximize the summed squared means.
double maximize(const moments_t& m, int dir, int first, int last, int& cut,
                int64_t whole_r, int64_t whole_g, int64_t whole_b, int64_t whole_w,
                const box_t& box)
{
    const int64_t base_r = bottom(m.mr.data(), dir, box);
    const int64_t base_g = bottom(m.mg.data(), dir, box);
    const int64_t base_b = bottom(m.mb.data(), dir, box);
    const int64_t base_w = bottom(m.wt.data(), dir, box);
    double best = 0.0;
    cut = -1;
    for (int i = first; i < last; ++i) {
        int64_t half_r = base_r + top(m.mr.data(), dir, i, box);
        int64_t half_g = base_g + top(m.mg.data(), dir, i, box);
        int64_t half_b = base_b + top(m.mb.data(), dir, i, box);
        int64_t half_w = base_w + top(m.wt.data(), dir, i, box);
        if (half_w == 0) continue;
        const double hr = static_cast<double>(half_r);
        const double hg = static_cast<double>(half_g);
        const double hb = static_cast<double>(half_b);
        double temp = (hr * hr + hg * hg + hb * hb) / static_cast<double>(half_w);

        half_r = whole_r - half_r;
        half_g = whole_g - half_g;
        half_b = whole_b - half_b;
        half_w = whole_w - half_w;
        if (half_w == 0) continue;
        const double tr = static_cast<double>(half_r);
        const double tg = static_cast<double>(half_g);
        const double tb = static_cast<double>(half_b);
        temp += (tr * tr + tg * tg + tb * tb) / static_cast<double>(half_w);

        if (temp > best) {
            best = temp;
            cut = i;
        }
    }
    return best;
}

bool cutBox(const moments_t& m, box_t& set1, box_t& set2)
{
    const int64_t whole_r = volume(m.mr.data(), set1);
    const int64_t whole_g = volume(m.mg.data(), set1);
    const int64_t whole_b = volume(m.mb.data(), set1);
    const int64_t whole_w = volume(m.wt.data(), set1);

    int cutr, cutg, cutb;
    const double maxr = maximize(m, kRed, set1.r0 + 1, set1.r1, cutr,
                                 whole_r, whole_g, whole_b, whole_w, set1);
    const double maxg = maximize(m, kGreen, set1.g0 + 1, set1.g1, cutg,
                                 whole_r, whole_g, whole_b, whole_w, set1);
    const double maxb = maximize(m, kBlue, set1.b0 + 1, set1.b1, cutb,
                                 whole_r, whole_g, whole_b, whole_w, set1);

    int dir;
    if (maxr >= maxg && maxr >= maxb) {
        dir = kRed;
        if (cutr < 0) return false;
    } else if (maxg >= maxr && maxg >= maxb) {
        dir = kGreen;
    } else {
        dir = kBlue;
    }

    set2.r1 = set1.r1;
    set2.g1 = set1.g1;
    set2.b1 = set1.b1;
    switch (dir) {
    case kRed:
        set2.r0 = set1.r1 = cutr;
        set2.g0 = set1.g0;
        set2.b0 = set1.b0;
        break;
    case kGreen:
        set2.g0 = set1.g1 = cutg;
        set2.r0 = set1.r0;
        set2.b0 = set1.b0;
        break;
    default:
        set2.b0 = set1.b1 = cutb;
        set2.r0 = set1.r0;
        set2.g0 = set1.g0;
        break;
    }
    set1.vol = (set1.r1 - set1.r0) * (set1.g1 - set1.g0) * (set1.b1 - set1.b0);
    set2.vol = (set2.r1 - set2.r0) * (set2.g1 - set2.g0) * (set2.b1 - set2.b0);
    return true;
}

void mark(const box_t& box, uint8_t label, std::vector<uint8_t>& tag)
{
    for (int r = box.r0 + 1; r <= box.r1; ++r)
        for (int g = box.g0 + 1; g <= box.g1; ++g)
            for (int b = box.b0 + 1; b <= box.b1; ++b)
                tag[lutIndex(r, g, b)] = label;
}

int nearestEntry(const int32_t rgba[4], const std::vector<uint8_t>& palette, int count)
{
    uint32_t best = 0xFFFFFFFFu;
    int best_id = 0;
    for (int i = 0; i < count; ++i) {
        const uint8_t* e = &palette[static_cast<size_t>(i) << 2];
        const int dr = rgba[0] - e[0];
        const int dg = rgba[1] - e[1];
        const int db = rgba[2] - e[2];
        const int da = rgba[3] - e[3];
        const uint32_t dist = static_cast<uint32_t>(dr * dr + dg * dg + db * db + 3 * da * da);
        if (dist < best) {
            best = dist;
            best_id = i;
        }
    }
    return best_id;
}

} // namespace

bool wuquant_quantize_core(const uint8_t* rgba, size_t len, uint32_t width,
                           int max_colors,
                           std::vector<uint8_t>& palette_out,
                           std::vector<uint8_t>& indexed_out,
                           int& pal_count_out)
{
    if (!rgba || len == 0 || max_colors < 1 || max_colors > kMaxColors)
        return false;

    moments_t m;
    buildHistogram(rgba, len, m);
    prefixMoments(m);

    // 0xFF is the caller's transparent slot: keep at most 255 palette entries.
    int k = std::min(max_colors, kMaxColors - 1);
    box_t boxes[kMaxColors];
    boxes[0].r1 = boxes[0].g1 = boxes[0].b1 = 32;
    boxes[0].vol = 32 * 32 * 32;
    double box_var[kMaxColors] = {};
    int next = 0;
    for (int i = 1; i < k; ++i) {
        if (cutBox(m, boxes[next], boxes[i])) {
            box_var[next] = boxes[next].vol > 1 ? variance(m, boxes[next]) : 0.0;
            box_var[i] = boxes[i].vol > 1 ? variance(m, boxes[i]) : 0.0;
        } else {
            box_var[next] = 0.0;
            --i;
        }
        next = 0;
        double best = box_var[0];
        for (int j = 1; j <= i; ++j) {
            if (box_var[j] > best) {
                best = box_var[j];
                next = j;
            }
        }
        if (best <= 0.0) {
            k = i + 1;
            break;
        }
    }

    std::vector<uint8_t> palette(static_cast<size_t>(k) << 2, 0);
    for (int i = 0; i < k; ++i) {
        const int64_t weight = volume(m.wt.data(), boxes[i]);
        if (weight <= 0) continue;
        uint8_t* e = &palette[static_cast<size_t>(i) << 2];
        e[0] = static_cast<uint8_t>(volume(m.mr.data(), boxes[i]) / weight);
        e[1] = static_cast<uint8_t>(volume(m.mg.data(), boxes[i]) / weight);
        e[2] = static_cast<uint8_t>(volume(m.mb.data(), boxes[i]) / weight);
        e[3] = static_cast<uint8_t>(volume(m.ma.data(), boxes[i]) / weight);
    }

    std::vector<uint8_t> tag(kHistSize, 0);
    for (int i = 0; i < k; ++i) mark(boxes[i], static_cast<uint8_t>(i), tag);

    std::vector<uint8_t> indexed(len, 0);
    if (width == 0 || width > len || len % width != 0) {
        for (size_t i = 0; i < len; ++i) {
            const uint8_t* p = &rgba[i << 2];
            int r = p[0], g = p[1], b = p[2];
            if (p[3] == 0) r = g = b = 0;
            indexed[i] = tag[lutIndex(binOf(r), binOf(g), binOf(b))];
        }
    } else {
        dither::atkinson_t diff(width);
        float org[4];
        int32_t irgba[4] = {0, 0, 0, 0};
        uint8_t crgba[4];
        int32_t pal[4];
        for (size_t y = 0; y < len; y += width, diff.end_row()) {
            diff.begin_row();
            for (uint32_t cnt = 0; cnt < width; ++cnt, diff.advance()) {
                const uint8_t* p = &rgba[(y + diff.x) << 2];
                const float sErr = diff.load(p, org);
                int id;
                if (sErr > dither::kErrThreshSkip) {
                    diff.add_error(org);
                    dither::clipPack(org, irgba, crgba);
                    id = nearestEntry(irgba, palette, k);
                } else {
                    int r = p[0], g = p[1], b = p[2];
                    if (p[3] == 0) r = g = b = 0;
                    id = tag[lutIndex(binOf(r), binOf(g), binOf(b))];
                }
                diff.clear();
                const uint8_t* e = &palette[static_cast<size_t>(id) << 2];
                for (int c = 0; c < 4; ++c) pal[c] = e[c];
                diff.diffuse(org, pal, y, len);
                indexed[y + diff.x] = static_cast<uint8_t>(id);
            }
        }
    }

    palette_out = std::move(palette);
    indexed_out = std::move(indexed);
    pal_count_out = k;
    return true;
}

} // namespace media
} // namespace opensup
