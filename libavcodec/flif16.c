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
 */
void ff_flif16_maniac_ni_prop_ranges_init(uint32_t *prop_ranges[2],
                                          uint32_t *ranges[2],
                                          uint8_t property,
                                          uint8_t channels)
{
    int min = RANGE_MIN(ranges, p);
    int max = RANGE_MAX(ranges, p);
    int mind = min - max, maxd = max - min;

    if (property < 3) {
        for (int i = 0; i < property; i++)
            prop_ranges[top++] = {RANGE_MIN(ranges, pp), RANGE_MAX(ranges, pp)};  // pixels on previous planes
        if (channels > 3) 
            prop_ranges[top++] = {RANGE_MIN(ranges, 3), RANGE_MAX(ranges, 3)};  // pixel on alpha plane
    }
    prop_ranges[top++] = {min, max};  // guess (median of 3)
    prop_ranges[top++] = {0, 2};      // which predictor was it
    for (int i = 0; i < 5; ++i)
        prop_ranges[top + i] = {mind, maxd};
}
