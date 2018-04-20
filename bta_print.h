/*
 * bta_print.h
 *
 * Copyright 2018 Edward V. Emelianov <eddy@sao.ru, edward.emelianoff@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#pragma once
#ifndef __BTA_PRINT_H__
#define __BTA_PRINT_H__

#include <fitsio.h>

/*
 * SAO longitude 41 26 29.175
 * SAO latitude 43 39 12.7
 * SAO altitude 2070
 * BTA focal ratio 24.024 m
 */
#ifndef TELLAT
    #define TELLAT  (43.6535278)
#endif
#ifndef TELLONG
    #define TELLONG (41.44143375)
#endif
#ifndef TELALT
    #define TELALT (2070.0)
#endif
#ifndef TELFOCUS
    #define TELFOCUS (24.024)
#endif

void write_bta_data(fitsfile *fp);

#endif // __BTA_PRINT_H__
