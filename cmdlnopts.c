/*
 * cmdlnopts.c - the only function that parse cmdln args and returns glob parameters
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
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include <limits.h>
#include "cmdlnopts.h"
#include "usefull_macros.h"

#define RAD 57.2957795130823
#define D2R(x) ((x) / RAD)
#define R2D(x) ((x) * RAD)

/*
 * here are global parameters initialisation
 */
static int help;
static glob_pars  G;

int rewrite_ifexists = 0, // rewrite existing files == 0 or 1
    verbose = 0; // each -v increments this value, e.g. -vvv sets it to 3
//            DEFAULTS
// default global parameters
glob_pars const Gdefault = {
    .outfile = "atik_out",
    .objtype = "object",
    .instrument = "direct imaging",
    .exptime = -1,
    .nframes = 1,
    .hbin = 1, .vbin = 1,
    .X0 = -1, .Y0 = -1,
    .X1 = -1, .Y1 = -1,
    .temperature = 1e6,
    .shtr_cmd = SHUTTER_LEAVE,
};

/*
 * Define command line options by filling structure:
 *  name    has_arg flag    val     type        argptr          help
*/
myoption cmdlnopts[] = {
    {"help",    NO_ARGS,    &help,   1,     arg_none,   NULL,               N_("show this help")},
    {"force",   NO_ARGS,    &rewrite_ifexists,1,arg_none,NULL,              N_("rewrite output file if exists")},
    {"verbose", NO_ARGS,    NULL,   'V',    arg_none,   APTR(&verbose),     N_("verbose level (each -V increase it)")},
    {"camname", NEED_ARG,   NULL,   'c',    arg_string, APTR(&G.camname),   N_("camera device name")},
    {"dark",    NO_ARGS,    NULL,   'd',    arg_int,    APTR(&G.dark),      N_("not open shutter, when exposing (\"dark frames\")")},
    {"open-shutter",NO_ARGS,&G.shtr_cmd,SHUTTER_OPEN,arg_none,NULL,     N_("open shutter")},
    {"close-shutter",NO_ARGS,&G.shtr_cmd,SHUTTER_CLOSE,arg_none,NULL,   N_("close shutter")},
    {"author",  NEED_ARG,   NULL,   'A',    arg_string, APTR(&G.author),    N_("program author")},
    {"objtype", NEED_ARG,   NULL,   'Y',    arg_string, APTR(&G.objtype),   N_("object type (neon, object, flat etc)")},
    {"instrument",NEED_ARG, NULL,   'I',    arg_string, APTR(&G.instrument),N_("instrument name")},
    {"object",  NEED_ARG,   NULL,   'O',    arg_string, APTR(&G.objname),   N_("object name")},
    {"obsname", NEED_ARG,   NULL,   'N',    arg_string, APTR(&G.observers), N_("observers' names")},
    {"prog-id", NEED_ARG,   NULL,   'P',    arg_string, APTR(&G.prog_id),   N_("observing program name")},
    {"hbin",    NEED_ARG,   NULL,   'h',    arg_int,    APTR(&G.hbin),      N_("horizontal binning to N pixels")},
    {"vbin",    NEED_ARG,   NULL,   'v',    arg_int,    APTR(&G.vbin),      N_("vertical binning to N pixels")},
    {"nframes", NEED_ARG,   NULL,   'n',    arg_int,    APTR(&G.nframes),   N_("make series of N frames")},
    {"pause",   NEED_ARG,   NULL,   'p',    arg_int,    APTR(&G.pause_len), N_("make pause for N seconds between expositions")},
    {"exptime", NEED_ARG,   NULL,   'x',    arg_double, APTR(&G.exptime),   N_("set exposure time to given value (seconds!!!)")},
    {"X0",      NEED_ARG,   NULL,   0,      arg_int,    APTR(&G.X0),        N_("frame X0 coordinate")},
    {"Y0",      NEED_ARG,   NULL,   0,      arg_int,    APTR(&G.Y0),        N_("frame Y0 coordinate")},
    {"X1",      NEED_ARG,   NULL,   0,      arg_int,    APTR(&G.X1),        N_("frame X1 coordinate")},
    {"Y1",      NEED_ARG,   NULL,   0,      arg_int,    APTR(&G.Y1),        N_("frame Y1 coordinate")},
    {"set-temp",NEED_ARG,   NULL,   't',    arg_double, APTR(&G.temperature),N_("set CCD temperature to given value (degr C)")},
    {"warmup",  NO_ARGS,    NULL,   'w',    arg_none,   APTR(&G.warmup),    N_("warm up CCD")},
    {"fast",    NO_ARGS,    NULL,   'f',    arg_none,   APTR(&G.fast),      N_("fast (8-bit) mode")},
    {"preview", NO_ARGS,    NULL,   'e',    arg_none,   APTR(&G.preview),   N_("preview mode")},
    end_option
};


/**
 * Parse command line options and return dynamically allocated structure
 *      to global parameters
 * @param argc - copy of argc from main
 * @param argv - copy of argv from main
 * @return allocated structure with global parameters
 */
glob_pars *parse_args(int argc, char **argv){
    void *ptr;
    ptr = memcpy(&G, &Gdefault, sizeof(G)); assert(ptr);
    // format of help: "Usage: progname [args]\n"
    change_helpstring("Usage: %s [args] <output file prefix>\n\n\tWhere args are:\n");
    // parse arguments
    parseargs(&argc, &argv, cmdlnopts);
    if(help) showhelp(-1, cmdlnopts);
    if(argc > 0){
        G.outfile = strdup(argv[0]);
        if(argc > 1){
            WARNX("%d unused parameters:\n", argc - 1);
            for(int i = 1; i < argc; ++i)
                printf("\t%4d: %s\n", i, argv[i]);
        }
    }
    return &G;
}

