// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "opensup/media/qtzrutc_core.h"

namespace opensup {
namespace media {

#define N_COMPONENTS 4
#define MAX_DEPTH 8
#define MAX_WIDTH 16
#define MAX_PALETTE_ENTRIES 256

#define DTYPE_ERROR float
#define POWFUN      powf

#define ERR_THRESH_SKIP (2)
#define MAX_ERROR_COMPONENT (14)
#define N_ACTIVE_ROWS_DIFFUSION (3)
#define ERROR_ROW(y, rl) ((y) % N_ACTIVE_ROWS_DIFFUSION)*(rl)
#define ERROR_ROW_LOC(y, rl, x) (ERROR_ROW(y, rl) + (x << 2))
#define ADDERROR(d, e)  (d)[0] += (e)[0];\
                        (d)[1] += (e)[1];\
                        (d)[2] += (e)[2];\
                        (d)[3] += (e)[3]
#define DEN_DITHERING (8)
#define DITHDIV(x) ((x) / (DTYPE_ERROR)DEN_DITHERING)

#define XBIT2ID(val, off, shift) (((val[shift] >> (MAX_DEPTH - 1 - off)) & 0x01) << shift)

/// Round to nearest, clamp to byte.
static inline uint32_t clipRound(DTYPE_ERROR value)
{
    const long r = lrint(value);
    return (uint32_t)(r > 255 ? 255 : (r < 0 ? 0 : r));
}

static void inline unpackRgba(const uint8_t *value, DTYPE_ERROR *prgba)
{
    prgba[3] = (DTYPE_ERROR)(value[3]);
    if (value[3] > 0) {
        prgba[2] = (DTYPE_ERROR)(value[2]);
        prgba[1] = (DTYPE_ERROR)(value[1]);
        prgba[0] = (DTYPE_ERROR)(value[0]);
    } else {
        prgba[2] = prgba[1] = prgba[0] = 0;
    }
}

static void inline unpackRgbaByte(const uint8_t *value, uint8_t *prgba)
{
    prgba[3] = (value[3]);
    if (value[3] > 0) {
        prgba[2] = (value[2]);
        prgba[1] = (value[1]);
        prgba[0] = (value[0]);
    } else {
        prgba[2] = prgba[1] = prgba[0] = 0;
    }
}

static void inline packPack(const uint32_t *rgba, int32_t *irgba, uint8_t *v)
{
    irgba[3] = v[3] = (uint8_t)rgba[3];
    if (irgba[3] > 0) {
        irgba[2] = v[2] = (uint8_t)rgba[2];
        irgba[1] = v[1] = (uint8_t)rgba[1];
        irgba[0] = v[0] = (uint8_t)rgba[0];
    } else {
        irgba[2] = v[2] = 0;
        irgba[1] = v[1] = 0;
        irgba[0] = v[0] = 0;
    }
}

static void inline clipPackPack(const DTYPE_ERROR *rgba, int32_t *irgba, uint8_t *v)
{
    irgba[3] = v[3] = (uint8_t)clipRound(rgba[3]);
    if (irgba[3] > 0) {
        irgba[2] = v[2] = (uint8_t)clipRound(rgba[2]);
        irgba[1] = v[1] = (uint8_t)clipRound(rgba[1]);
        irgba[0] = v[0] = (uint8_t)clipRound(rgba[0]);
    } else {
        irgba[2] = v[2] = 0;
        irgba[1] = v[1] = 0;
        irgba[0] = v[0] = 0;
    }
}

typedef struct rgba_leaf_s {
    uint8_t rgba[N_COMPONENTS];
    unsigned int count;
    uint16_t pid;
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
    int32_t paletteLUT[MAX_PALETTE_ENTRIES][N_COMPONENTS];
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

static void destroy(ctx_t *pctx)
{
    if (!pctx) return;
    free(pctx->leafs);
    pctx->nLeafsMax = pctx->nLeafs = 0;
    flush(pctx->root);
    free(pctx);
}

static ctx_t* init(void)
{
    ctx_t *ctx = (ctx_t*)calloc(1, sizeof(ctx_t));
    if (!ctx)
        return NULL;
    ctx->root = (hexnode_t*)calloc(1, sizeof(hexnode_t));
    if (!ctx->root) {
        free(ctx);
        return NULL;
    }
    return ctx;
}

static int insert(ctx_t *ctx, const uint8_t *value)
{
    hexnode_t *node = ctx->root;

    for (uint8_t d = 0, c; d < MAX_DEPTH; d++) {
        c = indexFrom(value, d);
        if (!node->children[c])
        {
            node->children[c] = (hexnode_t*)calloc(1, sizeof(hexnode_t));
            if (!node->children[c])
                return -1;
        }
        node = node->children[c];
    }
    if (!node->priv)
    {
        if (ctx->nLeafs >= ctx->nLeafsMax) {
            rgba_leaf_t **newleafs = (rgba_leaf_t**)realloc(ctx->leafs, sizeof(rgba_leaf_t*)*(ctx->nLeafsMax+1024));
            if (!newleafs) {
                return -1;
            }
            ctx->nLeafsMax += 1024;
            ctx->leafs = newleafs;
        }

        node->priv = (rgba_leaf_t*)calloc(1, sizeof(rgba_leaf_t));
        if (!node->priv)
            return -1;

        unpackRgbaByte(value, node->priv->rgba);
        node->priv->pid = MAX_PALETTE_ENTRIES;
        ctx->leafs[ctx->nLeafs++] = (rgba_leaf_t*)node->priv;
    }
    ++node->priv->count;

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
    if (!cur->priv)
        return -1;
    return (int32_t)cur->priv->pid;
}

static int32_t inline testFetchColourLeaf(hexnode_t *cur, const uint8_t *value)
{
    for (uint8_t d = 0, c; d < MAX_DEPTH; d++) {
        c = indexFrom(value, d);
        if (cur->children[c])
            cur = cur->children[c];
        else
            break;
    }
    if (cur->priv)
        return (int32_t)cur->priv->pid;
    return MAX_PALETTE_ENTRIES;
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

static inline int16_t findClosestInPalette(const ctx_t *ctx, const int32_t *irgba)
{
    uint32_t minDist = (uint32_t)(-1);
    int16_t bestFitId = -1;

    for (int16_t paletteEntryId = 0; paletteEntryId < ctx->lutLen; ++paletteEntryId) {
        uint32_t dist = colorDist(ctx->paletteLUT[paletteEntryId], irgba);

        int32_t diff = (ctx->paletteLUT[paletteEntryId][3] - irgba[3]);
        dist = dist + 3*(uint32_t)(diff*diff);
        if (dist < minDist) {
            minDist = dist;
            bestFitId = paletteEntryId;
        }
    }
    return bestFitId;
}

static int generateUnditheredBitmap(const ctx_t *ctx, uint8_t** bitmap, const uint8_t* rgba, const size_t len, const uint32_t plen)
{
    *bitmap = (uint8_t*)calloc(len, sizeof(uint8_t));
    if (NULL == *bitmap)
        return -1;

    uint32_t value;
    for (size_t k = 0; k < len; k++) {
        value = (uint32_t)fetchColour(ctx->root, &rgba[k<<2]);
        if (value >= plen)
            return -1;
        (*bitmap)[k] = (uint8_t)value;
    }
    return 0;
}

static int generateBitmap( const ctx_t* ctx, uint8_t** bitmap, const uint8_t* rgba,
                                   const size_t len, const int16_t plen, const uint32_t width )
{
    if (0 == width) {
        return generateUnditheredBitmap(ctx, bitmap, rgba, len, plen);
    }
    const uint32_t rowLen = width << 2;
    const uint32_t errArrLen = (width << 2) * N_ACTIVE_ROWS_DIFFUSION;

    *bitmap = (uint8_t*)malloc(len*sizeof(uint8_t));
    if (NULL == *bitmap)
        return -1;

    DTYPE_ERROR *pErrors, *errors = (DTYPE_ERROR*)calloc(errArrLen, sizeof(DTYPE_ERROR));
    if (NULL == errors)
        return -1;

    DTYPE_ERROR sErr, rgbaOrg[N_COMPONENTS], rgbaErr[N_COMPONENTS];
    int32_t paletteEntryId = 0, irgba[N_COMPONENTS] = {};
    uint8_t crgba[N_COMPONENTS];
    const uint8_t *rgbaPixel;

    int direction = -1;
    DTYPE_ERROR tmpErr;

    for (uint32_t y = 0, lineId = 0; y < len; y += width, ++lineId) {
        uint32_t x;
        if (direction < 0) {
            direction = 1;
            x = 0;
        } else {
            direction = -1;
            x = width - 1;
        }
        for (uint32_t cnt = 0; cnt < width; ++cnt, x += direction) {
            pErrors = &errors[ERROR_ROW_LOC(lineId, rowLen, x)];
            rgbaPixel = &rgba[(y + x) << 2];

            sErr = 0;
            for (uint8_t k = 0; k < N_COMPONENTS; k++) {
#define DTMAXERR (DTYPE_ERROR)MAX_ERROR_COMPONENT
                tmpErr = pErrors[k] < DTMAXERR ? pErrors[k] : DTMAXERR;
                tmpErr = tmpErr > (-1)*DTMAXERR ? tmpErr : (-1)*DTMAXERR;
#undef DTMAXERR
                sErr += tmpErr*tmpErr;
                pErrors[k] = tmpErr;
            }

            unpackRgba(rgbaPixel, rgbaOrg);
            if (sErr > (DTYPE_ERROR)ERR_THRESH_SKIP) {
                ADDERROR(rgbaOrg, pErrors);
                clipPackPack(rgbaOrg, irgba, crgba);
                paletteEntryId = testFetchColourLeaf(ctx->root, crgba);
            } else {
                paletteEntryId = fetchColour(ctx->root, rgbaPixel);
            }
            memset(pErrors, 0, sizeof(rgbaErr));

            if (paletteEntryId >= MAX_PALETTE_ENTRIES) {
                paletteEntryId = findClosestInPalette(ctx, irgba);
            } else if (paletteEntryId >= plen) {
                free(errors);
                return -1;
            }

            for (uint8_t k = 0; k < N_COMPONENTS; k++)
                rgbaErr[k] = DITHDIV(rgbaOrg[k] - (DTYPE_ERROR)ctx->paletteLUT[paletteEntryId][k]);

            if ((direction > 0 && x < width - 2) || (direction < 0 && x > 1)) {
                ADDERROR(pErrors + 4*direction, rgbaErr);
                ADDERROR(pErrors + 8*direction, rgbaErr);
            }
            if ((y < len - width) && (x < width - 1) && (x > 0)) {
                pErrors = &errors[ERROR_ROW_LOC(lineId+1, rowLen, x)];
                ADDERROR(pErrors - 4, rgbaErr);
                ADDERROR(pErrors,     rgbaErr);
                ADDERROR(pErrors + 4, rgbaErr);
            }
            if (y < len - 2*width) {
                pErrors = &errors[ERROR_ROW_LOC(lineId+2, rowLen, x)];
                ADDERROR(pErrors, rgbaErr);
            }
            (*bitmap)[y + x] = (uint8_t)paletteEntryId;
        }
    }
    free(errors);
    return 0;
}

typedef struct cluster_s {
    uint32_t rgba[N_COMPONENTS];
    uint32_t weight;
    uint32_t channelMaxError;
    uint32_t popCount;
    uint8_t channelIdMaxError;
    uint8_t cannotSplit;
    uint16_t cid;
    rgba_leaf_t **population;
} cluster_t;

#define ADD_WEIGHTED_RGBA(dest, v, w)    \
    dest[0] += w*(uint32_t)v[0];\
    dest[1] += w*(uint32_t)v[1];\
    dest[2] += w*(uint32_t)v[2];\
    dest[3] += w*(uint32_t)v[3]

#define ADD_DIFF_WEIGHTED_RGBA(dest, va, vb, w)       \
    dest[0] += w*absdiff((va)[0], (vb)[0]);\
    dest[1] += w*absdiff((va)[1], (vb)[1]);\
    dest[2] += w*absdiff((va)[2], (vb)[2]);\
    dest[3] += w*absdiff((va)[3], (vb)[3])

#define AVERAGE_RGBA(dest, w)            \
    do { DTYPE_ERROR dw = (DTYPE_ERROR)w;\
        dest[0] = clipRound(dest[0]/dw);\
        dest[1] = clipRound(dest[1]/dw);\
        dest[2] = clipRound(dest[2]/dw);\
        dest[3] = clipRound(dest[3]/dw);\
    } while (0)

static inline uint32_t absdiff(uint32_t a, uint32_t b)
{
    return a > b ? a - b : b - a;
}

static void get_cluster_stats(cluster_t *cluster)
{
    const rgba_leaf_t *member;

    cluster->weight = cluster->channelIdMaxError = cluster->channelMaxError = cluster->cannotSplit = 0;
    memset(cluster->rgba, 0, N_COMPONENTS*sizeof(uint32_t));

    for (uint32_t k = 0; k < cluster->popCount; ++k) {
        member = cluster->population[k];
        ADD_WEIGHTED_RGBA(cluster->rgba, member->rgba, member->count);
        cluster->weight += member->count;
    }
    if (0 == cluster->weight) {
        memset(cluster->rgba, 0, N_COMPONENTS*sizeof(uint32_t));
        return;
    }
    AVERAGE_RGBA(cluster->rgba, cluster->weight);

    // Transparent clusters stay (0,0,0,0).
    if (0 == cluster->rgba[3]) {
        cluster->rgba[0] = cluster->rgba[1] = cluster->rgba[2] = 0;
    }

    uint32_t rgba_diff[] = {0, 0, 0, 0};

    for (uint32_t k = 0; k < cluster->popCount; ++k) {
        member = cluster->population[k];
        ADD_DIFF_WEIGHTED_RGBA(rgba_diff, cluster->rgba, member->rgba, member->count);
    }

    cluster->channelIdMaxError = 0;
    for (int k = 1; k < N_COMPONENTS; ++k)
        if (rgba_diff[cluster->channelIdMaxError] < rgba_diff[k])
            cluster->channelIdMaxError = k;
    cluster->channelMaxError = clipRound(rgba_diff[cluster->channelIdMaxError]/(DTYPE_ERROR)cluster->weight);
}

static int split_cluster(cluster_t *cluster, cluster_t *neqtzrutc)
{
    if (cluster->cid >= MAX_PALETTE_ENTRIES || neqtzrutc->cid >= MAX_PALETTE_ENTRIES)
        return MAX_PALETTE_ENTRIES;
    const unsigned int chn = cluster->channelIdMaxError;
    const unsigned int chnMean = cluster->rgba[chn];

    unsigned int il = 0;
    unsigned int ir = cluster->popCount;

    unsigned int wl = 0, we = 0;

    rgba_leaf_t *tmp;
    for (unsigned int k = 0; k < ir; ++k) {
        if (cluster->population[k]->rgba[chn] > chnMean) {
            if (--ir != k) {
                tmp = cluster->population[ir];
                cluster->population[ir] = cluster->population[k];
                cluster->population[k] = tmp;
            }
            --k;
        } else if (cluster->population[k]->rgba[chn] < chnMean) {
            wl += cluster->population[k]->count;
            // Smaller values left, equals in the middle.
            tmp = cluster->population[il];
            cluster->population[il] = cluster->population[k];
            cluster->population[k] = tmp;
            ++il;
        } else {
            we += cluster->population[k]->count;
        }
    }

    // Equals join the smaller side when the larger one dominates.
    if (cluster->weight - we > 2*wl) {
        neqtzrutc->popCount = cluster->popCount - ir;
        neqtzrutc->population = &cluster->population[ir];
    } else {
        neqtzrutc->popCount = cluster->popCount - il;
        neqtzrutc->population = &cluster->population[il];
    }
    if (cluster->popCount > neqtzrutc->popCount) {
        cluster->popCount -= neqtzrutc->popCount;

        for (unsigned int k = 0; k < neqtzrutc->popCount; ++k)
            neqtzrutc->population[k]->pid = neqtzrutc->cid;

        get_cluster_stats(cluster);
        get_cluster_stats(neqtzrutc);
        return 1;
    }
    cluster->cannotSplit = 1;
    return 0;
}

static int find_perform_split(cluster_t *clusters, unsigned int *clusterCnt, const unsigned int maxClusters)
{
    const cluster_t *cluster = &clusters[0];

    int bestFit = -1;
    DTYPE_ERROR cost, highestCost = -1;
    DTYPE_ERROR power = 3/(DTYPE_ERROR)4 - ((DTYPE_ERROR)(*clusterCnt) + (DTYPE_ERROR)0.5) / (DTYPE_ERROR)(maxClusters * 2);

    for (unsigned int k = 0; k < *clusterCnt; ++k, ++cluster) {
        if (cluster->popCount > 1 && !cluster->cannotSplit) {
            cost = (DTYPE_ERROR)cluster->channelMaxError * POWFUN((DTYPE_ERROR)cluster->weight, power);
            if (cost > highestCost) {
                bestFit = k;
                highestCost = cost;
            }
        }
    }
    if (bestFit < 0)
        return 0;

    // split_cluster yields 256 on failure, pushing clusterCnt out of range.
    *clusterCnt += (unsigned int)split_cluster(&clusters[bestFit], &clusters[*clusterCnt]);
    return *clusterCnt <= maxClusters;
}

static int32_t clusterize(ctx_t *pctx, unsigned int maxClusters, uint8_t **palette)
{
    unsigned int clusterCnt = 1;
    cluster_t clusters[MAX_PALETTE_ENTRIES];
    memset(clusters, 0, sizeof(cluster_t)*MAX_PALETTE_ENTRIES);

    for (unsigned int k = 0; k < maxClusters; ++k)
        clusters[k].cid = (uint16_t)k;
    for (unsigned int k = maxClusters; k < MAX_PALETTE_ENTRIES; ++k)
        clusters[k].cid = MAX_PALETTE_ENTRIES;

    clusters[0].population = pctx->leafs;
    clusters[0].popCount = pctx->nLeafs;

    get_cluster_stats(&clusters[0]);
    for (unsigned int k = 0; k < clusters[0].popCount; ++k)
        clusters[0].population[k]->pid = 0;

    while (clusterCnt < maxClusters && find_perform_split(clusters, &clusterCnt, maxClusters));
    if (clusterCnt > maxClusters)
        return -1;

    *palette = (uint8_t*)calloc(clusterCnt << 2, sizeof(uint8_t));
    if (!*palette)
        return -1;

    uint8_t *pal = *palette;
    for (uint32_t k = 0; k < clusterCnt; ++k)
        packPack(clusters[k].rgba, pctx->paletteLUT[k], &pal[k<<2]);

    pctx->lutLen = (int16_t)clusterCnt;
    return (int32_t)clusterCnt;
}

bool qtzrutc_quantize_core(const uint8_t* rgba, size_t len, uint32_t width,
                           int max_colors,
                           std::vector<uint8_t>& palette_out,
                           std::vector<uint8_t>& indexed_out,
                           int& pal_count_out)
{
    palette_out.clear();
    indexed_out.clear();
    pal_count_out = 0;

    // 0xFF is reserved by the caller, so 255 is the hard cap.
    if (!rgba || len == 0) return false;
    if (max_colors < 1) return false;
    if (max_colors > 255) max_colors = 255;
    if (width > len || (width > 0 && len % width != 0)) return false;

    const size_t rawLen = len * N_COMPONENTS;
    int32_t err = 0;
    int32_t plen = 0;

    ctx_t *ctx = init();
    if (!ctx) return false;
    for (size_t k = 0; k < rawLen && 0 == err; k += N_COMPONENTS)
        err = insert(ctx, &rgba[k]);

    uint8_t *palette = NULL;
    uint8_t *bitmap = NULL;

    if (0 == err) {
        plen = clusterize(ctx, (unsigned int)max_colors, &palette);
        if (plen > 0 && plen <= MAX_PALETTE_ENTRIES)
            err = generateBitmap(ctx, &bitmap, rgba, len, (int16_t)plen, width);
        else
            err = 1;
    }

    destroy(ctx);

    if (0 != err || plen <= 0 || !palette || !bitmap) {
        free(palette);
        free(bitmap);
        return false;
    }

    palette_out.assign(palette, palette + (static_cast<size_t>(plen) << 2));
    indexed_out.assign(bitmap, bitmap + len);
    pal_count_out = plen;
    free(palette);
    free(bitmap);
    return true;
}

}
}
