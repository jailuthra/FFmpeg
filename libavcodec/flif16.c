/*
 * FLIF16 Image Format Definitions
 * Copyright (c) 2020 Anamitra Ghorui <aghorui@teknik.io>
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
 * FLIF16 format definitions and functions.
 */

#include "flif16.h"

/**
 * Initialise property ranges for non interlaced images.
 * @param[out] prop_ranges resultant ranges
 * @param[in]  color ranges of each channel
 * @param[in]  channels number of channels
 */
int32_t  (*ff_flif16_maniac_ni_prop_ranges_init(unsigned int *prop_ranges_size,
                                            FLIF16RangesContext *ranges,
                                            uint8_t property,
                                            uint8_t channels))[2]
{
    int min = ff_flif16_ranges_min(ranges, property);
    int max = ff_flif16_ranges_max(ranges, property);
    int mind = min - max, maxd = max - min;
    int32_t (*prop_ranges)[2];
    unsigned int top = 0;
    unsigned int size = (((property < 3) ? property : 0) + 2 + 5);
    *prop_ranges_size = size;
    prop_ranges = av_mallocz(sizeof(*prop_ranges) * size);
    if (property < 3) {
        for (int i = 0; i < property; i++) {
            prop_ranges[top][0]   = ff_flif16_ranges_min(ranges, i);
            prop_ranges[top++][1] = ff_flif16_ranges_max(ranges, i);  // pixels on previous planes
        }
        if (ranges->num_planes > 3)  {
            prop_ranges[top][0]   = ff_flif16_ranges_min(ranges, 3);
            prop_ranges[top++][1] = ff_flif16_ranges_max(ranges, 3);  // pixel on alpha plane
        }
    }
    prop_ranges[top][0]   = min;
    prop_ranges[top++][1] = max;  // guess (median of 3)
    prop_ranges[top][0]   = 0;
    prop_ranges[top++][1] = 2;      // which predictor was it
    for (int i = 0; i < 5; ++i) {
        prop_ranges[top][0] = mind;
        prop_ranges[top++][1] = maxd;
    }
    return prop_ranges;
}
/*
 *
 *     propRanges.clear();
    int min = ranges.min(p);
    int max = ranges.max(p);
    int mind = min - max, maxd = max - min;
    __PLN__
    MSG("pval: %d\n", p);
    if (p < 3) {
        for (int pp = 0; pp < p; pp++) {
            propRanges.push_back(std::make_pair(ranges.min(pp), ranges.max(pp)));  // pixels on previous planes
            __PLN__
        }
        if (ranges.numPlanes()>3) propRanges.push_back(std::make_pair(ranges.min(3), ranges.max(3)));  // pixel on alpha plane
    }
    propRanges.push_back(std::make_pair(min,max));   // guess (median of 3)
    propRanges.push_back(std::make_pair(0,2));       // which predictor was it
    __PLN__
    propRanges.push_back(std::make_pair(mind,maxd));
    propRanges.push_back(std::make_pair(mind,maxd));
    propRanges.push_back(std::make_pair(mind,maxd));
    propRanges.push_back(std::make_pair(mind,maxd));
    propRanges.push_back(std::make_pair(mind,maxd));
    __PLN__*/

