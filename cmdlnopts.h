/*
 * cmdlnopts.h - comand line options for parceargs
 *
 * Copyright 2013 Edward V. Emelianoff <eddy@sao.ru>
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
#ifndef __CMDLNOPTS_H__
#define __CMDLNOPTS_H__

#include "parseargs.h"

typedef enum{
    SHUTTER_LEAVE,
    SHUTTER_OPEN,
    SHUTTER_CLOSE
} shuttercmd;

/*
 * here are some typedef's for global data
 */
typedef struct{
    char *objname;      // object's name
    char *outfile;      // output filename prefix
    char *objtype;      // type of object (dark/obj/etc)
    char *instrument;   // instrument's name
    char *observers;    // observers' names
    char *prog_id;      // programm identificator
    char *author;       // programm author
    char *camname;      // camera name (if several connected)
    int warmup;         // warm up CCD
    int dark;           // dark frame
    double exptime;     // time of exposition in ms
    int nframes;        // amount of frames to take
    int hbin; int vbin; // binning
    int X0; int Y0;     // top left corner coordinate (-1 - all, including overscan)
    int X1; int Y1;     // bottom right corner coordinate
    int pause_len;      // pause (in seconds) between expositions
    int shtr_cmd;       // shutter command
    int fast;           // 8bit mode
    double temperature; // temperature of CCD
} glob_pars;

// default & global parameters
extern glob_pars const Gdefault;
extern int rewrite_ifexists, verbose;

glob_pars *parse_args(int argc, char **argv);
#endif // __CMDLNOPTS_H__
