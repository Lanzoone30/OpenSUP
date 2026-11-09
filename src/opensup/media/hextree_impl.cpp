// Copyright (C) 2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Port of brule's HexTree quantizer (MIT, (c) 2024 cubicibo).

// The Python/numpy binding was removed; the C++ core is unchanged.
//
// --- Original MIT license (cubicibo/brule) ---
/**
 MIT License
 Copyright (c) 2024, cubicibo
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
*/

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "opensup/media/hextree_impl.h"
#include "opensup/media/dither.hpp"

namespace dither = opensup::media::dither;

#define MAX_DEPTH 8
#define MAX_WIDTH 16
#define DTYPE_ERROR float

#define XBIT2ID(val, off, shift) (((val[shift] >> (MAX_DEPTH - 1 - off)) & 0x01) << shift)

typedef struct rgba_leaf_s {
    DTYPE_ERROR rgba[4];
    uint32_t count;
    void *ref;
    uint32_t depth;
} rgba_leaf_t;

typedef struct hexnode_s {
    rgba_leaf_t *priv;
    struct hexnode_s *children[MAX_WIDTH];
} hexnode_t;

typedef struct ctx_s {
    hexnode_t *root;
    rgba_leaf_t **leafs;
    uint32_t nLeafs;
    uint32_t nLeafsMax;
    uint32_t nEndLeafs;
    int32_t paletteLUT[256][4];
    int16_t lutLen;
} ctx_t;

static uint8_t inline indexFrom(const uint8_t *v, const uint8_t depth)
{
    return (XBIT2ID(v, depth, 0) | XBIT2ID(v, depth, 1) | XBIT2ID(v, depth, 2) | XBIT2ID(v, depth, 3)) * (v[3] > 0);
}

static void flush(hexnode_t *current)
{
    for (uint8_t c = 0; c < MAX_WIDTH; c++) {
        if (current->children[c])
            flush(current->children[c]);
    }
    if (current->priv)
        free(current->priv);
    free(current);
}

static void destroy(void *ctx)
{
    ctx_t *pctx = (ctx_t*)ctx;
    for (uint32_t k = 0; k < pctx->nLeafs; k++) {
        if (pctx->leafs[k]) {
            if (pctx->leafs[k]->ref)
                ((hexnode_t*)pctx->leafs[k]->ref)->priv = NULL;
            free(pctx->leafs[k]);
        }
    }
    free(pctx->leafs);
    pctx->nLeafsMax = pctx->nLeafs = 0;
    flush(pctx->root);
    free(ctx);
}

static void* init(void)
{
    ctx_t *ctx = (ctx_t*)calloc(1, sizeof(ctx_t));
    ctx->root = (hexnode_t*)calloc(1, sizeof(hexnode_t));
    return (void*)ctx;
}

static int insert(void *vctx, const uint8_t *value)
{
    ctx_t *ctx = (ctx_t*)vctx;

    hexnode_t *child;
    hexnode_t *node = ctx->root;

    for (uint8_t d = 0, c; d < MAX_DEPTH; d++) {
        c = indexFrom(value, d);
        if (!node->children[c])
        {
            child = (hexnode_t*)calloc(1, sizeof(hexnode_t));
            if (!child)
                return -1;

            if (ctx->nLeafs >= ctx->nLeafsMax) {
                rgba_leaf_t **newleafs = (rgba_leaf_t**)realloc(ctx->leafs, sizeof(rgba_leaf_t*)*(ctx->nLeafsMax+1024));
                if (!newleafs) {
                    free(child);
                    return -1;
                }
                ctx->nLeafsMax += 1024;
                ctx->leafs = newleafs;
            }

            child->priv = (rgba_leaf_t*)calloc(1, sizeof(rgba_leaf_t));
            if (!child->priv) {
                free(child);
                return -1;
            }
            child->priv->depth = d;
            child->priv->ref = (void*)child;

            dither::unpackRgba(value, child->priv->rgba);

            ctx->leafs[ctx->nLeafs++] = (rgba_leaf_t*)child->priv;
            node->children[c] = child;
            ctx->nEndLeafs += (d == MAX_DEPTH-1);
        }
        node = node->children[c];
        ++node->priv->count;
    }
    return 0;
}

static int32_t inline fetchColour(hexnode_t *cur, const uint8_t *value)
{
    for (uint8_t d = 0, c; d < MAX_DEPTH; d++) {
        c = indexFrom(value, d);
        if (cur->children[c])
            cur = cur->children[c];
        else
            break;
    }
    return (int32_t)cur->priv->count;
}

static int32_t inline testFetchColourLeaf(hexnode_t *cur, uint8_t *value)
{
    for (uint8_t d = 0, c; d < MAX_DEPTH; d++) {
        c = indexFrom(value, d);
        if (cur->children[c])
            cur = cur->children[c];
        else
            break;
    }
    //is leaf?
    if (!memcmp(&cur->children[1], cur->children, sizeof(hexnode_t*)*(MAX_WIDTH-1)))
        return (int32_t)cur->priv->count;
    return -1;
}

static inline uint32_t colorDist(const int32_t *v1, const int32_t *v2)
{
    int32_t diff = (v1[0] - v2[0]);
    uint32_t dist = diff*diff;
    diff = (v1[1] - v2[1]);
    dist += diff*diff;
    diff = (v1[2] - v2[2]);
    dist += diff*diff;
    return dist;
}

static inline int16_t findClosestInPalette(const ctx_t *ctx, const int32_t *irgba/*, int16_t penalizedEntry*/)
{
    uint32_t minDist = (uint32_t)(-1);
    int16_t bestFitId = -1;
    for (int16_t paletteEntryId = 0; paletteEntryId < ctx->lutLen; ++paletteEntryId) {
        uint32_t dist = colorDist(ctx->paletteLUT[paletteEntryId], irgba);
        int32_t diff = (ctx->paletteLUT[paletteEntryId][3] - irgba[3]);
        //dist = (uint32_t)((diff*diff)/(DTYPE_ERROR)1.5 + ((DTYPE_ERROR)dist*((irgba[3]*ctx->paletteLUT[paletteEntryId][3]/(DTYPE_ERROR)1953810))));
        dist = dist + 3*(uint32_t)(diff*diff);
        /*
        if (paletteEntryId == penalizedEntry)
            dist += (dist >> 4); //Penalize by 10%
        */
        if (dist < minDist) {
            minDist = dist;
            bestFitId = paletteEntryId;
        }
    }
    return bestFitId;
}

static int generateDitheredBitmap(const void *vctx, uint8_t **bitmap, const uint8_t *rgba,
                                   const size_t len, const int16_t plen, const uint32_t width)
{
    const ctx_t *ctx = (const ctx_t*)vctx;
    *bitmap = (uint8_t*)malloc(len*sizeof(uint8_t));
    if (NULL == *bitmap)
        return -1;
    dither::atkinson_t diff(width);
    float rgbaOrg[4];
    int32_t irgba[4] = {0, 0, 0, 0};
    uint8_t crgba[4];
    for (size_t y = 0; y < len; y += width, diff.end_row()) {
        diff.begin_row();
        for (uint32_t cnt = 0; cnt < width; ++cnt, diff.advance()) {
            const uint8_t *rgbaPixel = &rgba[(y + diff.x) << 2];
            const float sErr = diff.load(rgbaPixel, rgbaOrg);
            int32_t paletteEntryId;
            if (sErr > dither::kErrThreshSkip) {
                //Add the residual error to the image pixel
                diff.add_error(rgbaOrg);
                dither::clipPack(rgbaOrg, irgba, crgba);
                //Test if we have a match in the tree, returns -1 if none
                paletteEntryId = testFetchColourLeaf(ctx->root, crgba);
            } else {
                //no error => colour is a leaf
                paletteEntryId = fetchColour(ctx->root, rgbaPixel);
            }
            //reset error accumulator of pixel
            diff.clear();

            if (paletteEntryId < 0) {
                paletteEntryId = findClosestInPalette(ctx, irgba/*, fetchColour(ctx->root, rgbaPixel)*/);
            } else if (paletteEntryId >= plen) {
                return -1;
            }

            diff.diffuse(rgbaOrg, ctx->paletteLUT[paletteEntryId], y, len);
            (*bitmap)[y + diff.x] = paletteEntryId;
        }
    }
    return 0;
}

static int generateBitmap(const void* vctx, uint8_t** bitmap, const uint8_t* rgba, const size_t len, const uint32_t plen)
{
    const ctx_t *ctx = (const ctx_t*)vctx;

    *bitmap = (uint8_t*)calloc(len, sizeof(uint8_t));
    if (NULL == *bitmap)
        return -1;

    uint32_t value;
    for (size_t k = 0; k < len; k++) {
        value = (uint32_t)fetchColour(ctx->root, &rgba[k<<2]);

        //All pixels are unmodified, all tree look-ups shall always lend on a leaf (value < plen)
        if (value >= plen)
            return -1;
        (*bitmap)[k] = (uint8_t)value;
    }
    return 0;
}

static int cmpleafs(const void* e1, const void* e2)
{
    const rgba_leaf_t *l1 = *(rgba_leaf_t**)(void**)e1;
    const rgba_leaf_t *l2 = *(rgba_leaf_t**)(void**)e2;

    int r = (int)l2->depth - (int)l1->depth;
    if (r != 0)
        return r;
    return (int)l1->count - (int)l2->count;
}

static int32_t reduceTo(void *ctx, unsigned int maxLeafs, uint8_t **palette)
{
    ctx_t *pctx = (ctx_t*)ctx;
    hexnode_t *ref;
    rgba_leaf_t* leaf;

    if (maxLeafs < MAX_WIDTH || !pctx->leafs)
        return -1;

    qsort(pctx->leafs, pctx->nLeafs, sizeof(rgba_leaf_t*), cmpleafs);

    uint32_t leafCnt = pctx->nEndLeafs;
    for (uint32_t k = 0; k < pctx->nLeafs && maxLeafs < leafCnt; k++) {
        leaf = ((rgba_leaf_t*)pctx->leafs[k]);
        if (leaf->depth == MAX_DEPTH-1)
            continue;
        ref = (hexnode_t*)leaf->ref;
        if (!ref || !ref->priv)
            continue;

        memset(leaf->rgba, 0, sizeof(DTYPE_ERROR)*4);
        for (uint8_t cc = 0; cc < MAX_WIDTH; cc++) {
            if (ref->children[cc]) {
                DTYPE_ERROR cf = (DTYPE_ERROR)ref->children[cc]->priv->count / (DTYPE_ERROR)leaf->count;
                leaf->rgba[0] += ref->children[cc]->priv->rgba[0] * cf;
                leaf->rgba[1] += ref->children[cc]->priv->rgba[1] * cf;
                leaf->rgba[2] += ref->children[cc]->priv->rgba[2] * cf;
                leaf->rgba[3] += ref->children[cc]->priv->rgba[3] * cf;
                ref->children[cc]->priv->ref = NULL;
                free(ref->children[cc]);
                ref->children[cc] = NULL;
                --leafCnt;
            }
        }
        //parent is now a leaf
        ++leafCnt;
    }

    *palette = (uint8_t*)calloc(leafCnt << 2, sizeof(uint8_t));
    if (!palette)
        return -1;

    uint8_t *pal = *palette;
    for (uint32_t k = 0, plen = 0; plen != leafCnt; k++) {
        leaf = (rgba_leaf_t*)pctx->leafs[k];
        ref = (hexnode_t*)leaf->ref;

        if (leaf && ref) {
            dither::clipPack(leaf->rgba, pctx->paletteLUT[plen], &pal[plen << 2]);
            leaf->count = plen;
            ++plen;
        }
    }
    pctx->lutLen = leafCnt;
    return leafCnt;
}

namespace opensup {
namespace media {

bool hextree_quantize_core(const uint8_t* rgba, size_t len, uint32_t width,
                           int max_colors,
                           std::vector<uint8_t>& palette_out,
                           std::vector<uint8_t>& indexed_out,
                           int& pal_count_out)
{
    if (!rgba || max_colors < 16 || max_colors > 256)
        return false;

    void* ctx = init();
    if (!ctx)
        return false;
    int err = 0;
    for (size_t k = 0; k < len * 4 && err == 0; k += 4)
        err = insert(ctx, &rgba[k]);

    uint8_t* palette = nullptr;
    uint8_t* bitmap = nullptr;
    int plen = 0;
    if (err == 0) {
        plen = reduceTo(ctx, static_cast<unsigned int>(max_colors), &palette);
        if (plen > 0 && plen <= 256) {
            if (width)
                err = generateDitheredBitmap(ctx, &bitmap, rgba, len, (int16_t)plen, width);
            else
                err = generateBitmap(ctx, &bitmap, rgba, len, plen);
        } else {
            err = 1;
        }
    }
    destroy(ctx);

    if (err != 0 || !palette || !bitmap) {
        free(palette);
        free(bitmap);
        return false;
    }

    palette_out.assign(palette, palette + static_cast<size_t>(plen) * 4);
    indexed_out.assign(bitmap, bitmap + len);
    pal_count_out = plen;

    free(palette);
    free(bitmap);
    return true;
}

}
}
