/*
 * FLIF16 Image Format Definitions
 * Copyright (c) 2020 Anamitra Ghorui
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
 *//*
void ff_flif16_maniac_ni_prop_ranges_init(int32_t (*prop_ranges)[2],
                                          unsigned int *prop_ranges_size,
                                          int32_t (*ranges)[2],
                                          uint8_t property,
                                          uint8_t channels)
{
    int min = RANGE_MIN(ranges, channels, property);
    int max = RANGE_MAX(ranges, channels, property);
    int mind = min - max, maxd = max - min;
    unsigned int top = 0;
    unsigned int size = (((property < 3) ? 3 : 0) + 2 + 5);
    *prop_ranges_size = size;
    if (!prop_ranges)
        prop_ranges = av_mallocz(sizeof(*prop_ranges) * size);
    if (property < 3) {
        for (int i = 0; i < property; i++)
            RANGE_SET(prop_ranges[top++], RANGE_MIN(ranges, channels, i), 
                      RANGE_MAX(ranges, channels, i));  // pixels on previous planes
        if (channels > 3) 
            RANGE_SET(prop_ranges[top++], RANGE_MIN(ranges, channels, 3),
                      RANGE_MAX(ranges, channels, 3));  // pixel on alpha plane
    }
    RANGE_SET(prop_ranges[top++], min, max);  // guess (median of 3)
    RANGE_SET(prop_ranges[top++], 0, 2);      // which predictor was it
    for (int i = 0; i < 5; ++i)
        RANGE_SET(prop_ranges[top++], mind, maxd);
}
*/
