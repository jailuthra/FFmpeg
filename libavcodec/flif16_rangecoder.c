/*
 * Range coder for FLIF16
 * Copyright (c) 2004, Michael Niedermayer,
 *               2010-2016, Jon Sneyers & Pieter Wuille,
 *               2020, Anamitra Ghorui <aghorui@teknik.io>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

 /**
  * @file
  * Range coder for FLIF16
  */

#include "avcodec.h"
#include "libavutil/common.h"
#include "flif16_rangecoder.h"
#include "flif16.h"

// TODO write separate function for RAC decoder

// The coder requires a certain number of bytes for initiialization. buf
// provides it. gb is used by the coder functions for actual coding.
FLIF16RangeCoder *ff_flif16_rac_init(GetByteContext *gb, 
                                     uint8_t *buf,
                                     uint8_t buf_size)
{
    FLIF16RangeCoder *rc = av_mallocz(sizeof(*rc));
    GetByteContext gbi;

    if (!rc)
        return NULL;
    
    if(buf_size < FLIF16_RAC_MAX_RANGE_BYTES)
        return NULL;
    
    bytestream2_init(&gbi, buf, buf_size);

    rc->range  = FLIF16_RAC_MAX_RANGE;
    rc->gb     = gb;

    for (uint32_t r = FLIF16_RAC_MAX_RANGE; r > 1; r >>= 8) {
        rc->low <<= 8;
        rc->low |= bytestream2_get_byte(&gbi);
    }
    printf("[%s] low = %d\n", __func__, rc->low);
    return rc;
}

void ff_flif16_rac_free(FLIF16RangeCoder *rc)
{
    if (!rc)
        return;
    av_freep(rc);
}

// TODO Maybe restructure rangecoder.c/h to fit a more generic case
static void build_table(uint16_t *zero_state, uint16_t *one_state, size_t size,
                        uint32_t factor, unsigned int max_p)
{
    const int64_t one = 1LL << 32;
    int64_t p = one / 2;
    unsigned int last_p8 = 0, p8;
    unsigned int i;

    for (i = 0; i < size / 2; i++) {
        p8 = (size * p + one / 2) >> 32;
        if (p8 <= last_p8) 
            p8 = last_p8 + 1;
        if (last_p8 && last_p8 < size && p8 <= max_p)
            one_state[last_p8] = p8;
        p += ((one - p) * factor + one / 2) >> 32;
        last_p8 = p8;
    }

    for (i = size - max_p; i <= max_p; i++) {
        if (one_state[i])
            continue;
        p = (i * one + size / 2) / size;
        p += ((one - p) * factor + one / 2) >> 32;
        p8 = (size * p + one / 2) >> 32; //FIXME try without the one
        if (p8 <= i) 
            p8 = i + 1;
        if (p8 > max_p) 
            p8 = max_p;
        one_state[i] = p8;
    }

    for (i = 1; i < size; i++)
        zero_state[i] = size - one_state[size - i];
}

static inline uint32_t log4kf(int x, uint32_t base)
{
    int bits     = 8 * sizeof(int) - ff_clz(x);
    uint64_t y   = ((uint64_t)x) << (32 - bits);
    uint32_t res = base * (13 - bits);
    uint32_t add = base;
    while ((add > 1) && ((y & 0x7FFFFFFF) != 0)) {
        y = (((uint64_t)y) * y + 0x40000000) >> 31;
        add >>= 1;
        if ((y >> 32) != 0) {
            res -= add;
            y >>= 1;
        }
    }
    return res;
}

void ff_flif16_build_log4k_table(FLIF16Log4kTable *log4k)
{
    log4k->table[0] = 0;
    for (int i = 1; i < 4096; i++)
        log4k->table[i] = (log4kf(i, (65535UL << 16) / 12) + 
                          (1 << 15)) >> 16;
    log4k->scale = 65535 / 12;
}

void ff_flif16_chancetable_init(FLIF16ChanceTable *ct, int alpha, int cut)
{
    build_table(ct->zero_state, ct->one_state, 4096, alpha, 4096 - cut);
}

FLIF16ChanceContext *ff_flif16_chancecontext_init(void)
{
    FLIF16ChanceContext *ctx = av_malloc(sizeof(flif16_nz_int_chances));
    if(!ctx)
        return NULL;
    memcpy(ctx->data, &flif16_nz_int_chances, sizeof(flif16_nz_int_chances));
    return ctx;
}

#ifdef MULTISCALE_CHANCES_ENABLED
// TODO write free function
FLIF16MultiscaleChanceTable *ff_flif16_multiscale_chancetable_init(void)
{
    unsigned int len = MULTISCALE_CHANCETABLE_DEFAULT_SIZE;
    FLIF16MultiscaleChanceTable *ct = av_malloc(sizeof(*ct));
    for (int i = 0; i < len; ++i) {
        ff_flif16_chancetable_init(&ct->sub_table[i],
                                   flif16_multiscale_alphas[i],
                                   MULTISCALE_CHANCETABLE_DEFAULT_CUT);
    }
    return ct;
}

/**
 * Allocate and set all chances according to flif16_nz_int_chances
 */
FLIF16MultiscaleChanceContext *ff_flif16_multiscale_chancecontext_init(void)
{
    FLIF16MultiscaleChanceContext *ctx = av_malloc(sizeof(*ctx));
    for (int i = 0; i < sizeof(flif16_nz_int_chances) /
                        sizeof(flif16_nz_int_chances[0]); ++i)
        ff_flif16_multiscale_chance_set(&ctx->data[i], flif16_nz_int_chances[i]);
    return ctx;
}

#endif

// TODO write free function for forest
int ff_flif16_read_maniac_tree(FLIF16RangeCoder *rc,
                               FLIF16MANIACContext *m,
                               int32_t (*prop_ranges)[2],
                               unsigned int prop_ranges_size,
                               unsigned int channel)
{
    FLIF16MANIACTree  *tree;
    FLIF16MANIACNode  *curr_node;
    FLIF16MANIACStack *curr_stack;
    int p, oldmin, oldmax, split_val;

    if (!(m->forest[channel])) {
        m->forest[channel] = av_malloc(sizeof(*tree));
        if (!(m->forest[channel]))
            return AVERROR(ENOMEM);
        m->forest[channel]->data  = av_mallocz(MANIAC_TREE_BASE_SIZE *
                                               sizeof(*(m->forest[channel]->data)));
        tree = m->forest[channel];
        if (!tree->data) {
            return AVERROR(ENOMEM);
        }
        m->stack = av_malloc(MANIAC_TREE_BASE_SIZE * sizeof(*(m->stack)));
        if (!((tree->data) && (m->stack)))
            return AVERROR(ENOMEM);
        for (int i = 0; i < 3; ++i) {
            #ifdef MULTISCALE_CHANCES_ENABLED
            m->ctx[i] = ff_flif16_multiscale_chancecontext_init();
            #else
            m->ctx[i] = ff_flif16_chancecontext_init();
            #endif

            if (!m->ctx[i]) {
                __PLN__
            }
        }
        m->stack_top = m->tree_top = 0;
        tree->size = MANIAC_TREE_BASE_SIZE;
        m->stack[m->stack_top].id   = 0;
        m->stack[m->stack_top].mode = 0;
        ++m->stack_top;
    } else {
        return AVERROR(EINVAL);
    }

    switch (rc->segment) {
        case 0:
            start:
            if(!m->stack_top)
                goto end;
            curr_stack = &m->stack[m->stack_top - 1];
            curr_node  = &tree->data[curr_stack->id];
            p = curr_stack->p;
            __PLN__
            if(!curr_stack->visited){
                switch (curr_stack->mode) {
                    case 1:
                        RANGE_SET(prop_ranges[p], curr_stack->min,
                                  curr_stack->max);
                        break;

                    case 2:
                        prop_ranges[p][0] = curr_stack->min;
                        break;
                }
            } else {
                prop_ranges[p][1] = curr_stack->max2;
                --m->stack_top;
                goto start;
            }
            curr_stack->visited = 1;
            ++rc->segment;

        case 1:
            // int p = tree[stack[top]].property = coder[0].read_int2(0,nb_properties) - 1;
            #ifdef MULTISCALE_CHANCES_ENABLED
            RAC_GET(rc, m->ctx[0], 0, prop_ranges_size, &curr_node->property,
                    FLIF16_RAC_GNZ_MULTISCALE_INT);
            #else
            RAC_GET(rc, m->ctx[0], 0, prop_ranges_size, &curr_node->property,
                    FLIF16_RAC_GNZ_INT);
            #endif
            
            p = --(curr_node->property);

            if (p == -1) {
                __PLN__
                --m->stack_top;
                goto start;
            }
            curr_node->child_id = m->tree_top;

            oldmin = prop_ranges[p][0];
            oldmax = prop_ranges[p][1];
            if (oldmin >= oldmax)
                return AVERROR(EINVAL);
            ++rc->segment;

        case 2:
            //tree[stack[top]].count = coder[1].read_int2(CONTEXT_TREE_MIN_COUNT,
            //                                            CONTEXT_TREE_MAX_COUNT);
            #ifdef MULTISCALE_CHANCES_ENABLED
            RAC_GET(rc, m->ctx[1], MANIAC_TREE_MIN_COUNT, MANIAC_TREE_MAX_COUNT,
                    &curr_node->count, FLIF16_RAC_GNZ_MULTISCALE_INT);
            #else
            RAC_GET(rc, m->ctx[1], MANIAC_TREE_MIN_COUNT, MANIAC_TREE_MAX_COUNT,
                    &curr_node->count, FLIF16_RAC_GNZ_INT);
            #endif
            ++rc->segment;

        case 3:
            // int splitval = n.splitval = coder[2].read_int2(oldmin, oldmax-1);
            #ifdef MULTISCALE_CHANCES_ENABLED
            RAC_GET(rc, m->ctx[2], oldmin, oldmax - 1, &curr_node->split_val,
                    FLIF16_RAC_GNZ_MULTISCALE_INT);

            #else
            RAC_GET(rc, m->ctx[2], oldmin, oldmax - 1, &curr_node->split_val,
                    FLIF16_RAC_GNZ_INT);
            #endif
            split_val = curr_node->split_val;
            ++rc->segment;

        case 4:
            if ((m->tree_top) >= tree->size) {
                av_realloc(tree->data, (tree->size) * 2);
                if(!(tree->data))
                    return AVERROR(ENOMEM);
            }
            tree->size *= 2;

            if ((m->stack_top) >= m->stack_size) {
                av_realloc(m->stack, (m->stack_size) * 2);
                if(!(m->stack))
                    return AVERROR(ENOMEM);
            }
            m->stack_size *= 2;
            
            // WHEN GOING BACK UP THE TREE
            curr_stack->max2 = oldmax;

            // PUSH 1
            // <= splitval
            // subrange[p].first = oldmin;
            // subrange[p].second = splitval;
            // if (!read_subtree(childID + 1, subrange, tree)) return false;
            // TRAVERSE CURR + 2 (RIGHT CHILD)
            m->stack[m->stack_top].id      = m->tree_top + 1;
            m->stack[m->stack_top].p       = p;
            m->stack[m->stack_top].min     = oldmin;
            m->stack[m->stack_top].max     = split_val;
            m->stack[m->stack_top].mode    = 1;
            m->stack[m->stack_top].visited = 0;
            ++m->stack_top;

            // PUSH 2
            // > splitval
            // subrange[p].first = splitval+1;
            // if (!read_subtree(childID, subrange, tree)) return false;
            // TRAVERSE CURR + 1 (LEFT CHILD)
            m->stack[m->stack_top].id      = m->tree_top;
            m->stack[m->stack_top].p       = p;
            m->stack[m->stack_top].min     = oldmin;
            m->stack[m->stack_top].mode    = 2;
            m->stack[m->stack_top].visited = 0;
            ++m->stack_top;

            m->tree_top += 2;

            goto start;
    }

    end:
    av_realloc(tree->data, m->tree_top); // Maybe replace by fast realloc
    if (!tree->data)
        return AVERROR(ENOMEM);
    tree->size = m->tree_top;
    av_freep(m->stack);
    for (int i = 0; i < 3; ++i)
        av_free(m->ctx[i]);
    // return 0;
    return AVERROR_EOF;

    need_more_data:
    return AVERROR(EAGAIN);
}


// Properties properties((nump>3?NB_PROPERTIES_scanlinesA[p]:NB_PROPERTIES_scanlines[p]));

#ifdef MULTISCALE_CHANCES_ENABLED
FLIF16MultiscaleChanceContext *ff_flif16_maniac_findleaf(FLIF16MANIACContext *m,
                                                         uint8_t channel,
                                                         int32_t *properties)
#else
FLIF16ChanceContext *ff_flif16_maniac_findleaf(FLIF16MANIACContext *m,
                                               uint8_t channel,
                                               int32_t *properties)
#endif
{
    unsigned int pos = 0;
    uint32_t old_leaf;
    uint32_t new_leaf;
    FLIF16MANIACTree *tree = m->forest[channel];
    FLIF16MANIACNode *nodes = tree->data;

    #ifdef MULTISCALE_CHANCES_ENABLED
    FLIF16MultiscaleChanceContext *leaves;
    #else
    FLIF16ChanceContext *leaves;
    #endif

    if (m->forest[channel]->leaves) {
        m->forest[channel]->leaves = av_mallocz(MANIAC_TREE_BASE_SIZE *
                                                sizeof(*m->forest[channel]->leaves));
        m->forest[channel]->leaves_size = MANIAC_TREE_BASE_SIZE;
    }
    
    leaves = m->forest[channel]->leaves;

    while (nodes[pos].property != -1) {
        if (nodes[pos].count < 0) {
            if (properties[nodes[pos].property] > nodes[pos].split_val)
                pos = nodes[pos].child_id;
            else
                pos = nodes[pos].child_id + 1;
        } else if (nodes[pos].count > 0) {
            // assert(inner_node[pos].leafID >= 0);
            // assert((unsigned int)inner_node[pos].leafID < leaf_node.size());
            --nodes[pos].count;
            break;
        } else { // count == 0
            --nodes[pos].count;
            if ((tree->leaves_top) >= tree->leaves_size) {
                av_realloc(m->forest[channel]->leaves,
                           m->forest[channel]->leaves_size * 2);
                if (!m->forest[channel]->leaves)
                    return NULL;
                m->forest[channel]->leaves_size *= 2;
            }

            old_leaf = nodes[pos].leaf_id;
            new_leaf = tree->leaves_top;
            memcpy(&leaves[tree->leaves_top], &leaves[nodes[pos].leaf_id],
                   sizeof(*leaves));

            nodes[nodes[pos].child_id].leaf_id = old_leaf;
            nodes[nodes[pos].child_id + 1].leaf_id = new_leaf;

            if (properties[nodes[pos].property] > nodes[pos].split_val)
                return &leaves[old_leaf];
            else
                return &leaves[new_leaf];
        }
    }
    return &leaves[nodes[pos].leaf_id];
}

int ff_flif16_maniac_read_int(FLIF16RangeCoder *rc,
                              FLIF16MANIACContext *m,
                              int32_t *properties,
                              uint8_t channel,
                              int min, int max, int *target)
{
    if (!rc->maniac_ctx)
        rc->segment2 = 0;

    switch(rc->segment2) {
        case 0:
            rc->maniac_ctx = ff_flif16_maniac_findleaf(m, channel, properties);
            if(!rc->maniac_ctx)
                return AVERROR(ENOMEM);
            ++rc->segment2;

        case 1:
            #ifdef MULTISCALE_CHANCES_ENABLED
            RAC_GET(rc, rc->maniac_ctx, min, max, target, FLIF16_RAC_NZ_MULTISCALE_INT);
            #else
            RAC_GET(rc, rc->maniac_ctx, min, max, target, FLIF16_RAC_NZ_INT);
            #endif
            
    }

    rc->maniac_ctx = NULL;
    return 1;

    need_more_data:
        return 0;
}
