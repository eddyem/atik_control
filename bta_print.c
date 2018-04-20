/*
 * bta_print.c
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

/* Print some BTA NewACS data (or write  to file)
 * Usage:
 *         bta_print [time_step] [file_name]
 * Where:
 *         time_step - writing period in sec., >=1.0
 *                      <1.0 - once and exit (default)
 *         file_name - name of file to write to,
 *                      "-" - stdout (default)
 */
#ifdef USE_BTA
#include <time.h>
#include "bta_shdata.h"
#include "bta_print.h"
#include "main.h"

char comment[FLEN_CARD];
#define CMNT(...) snprintf(comment, FLEN_CARD, __VA_ARGS__)
#define FTKEY(...) WRITEKEY(fp, __VA_ARGS__, comment)

int shm_ready = FALSE; // BTA shm_get

static char buf[1024];
char *time_asc(double t){
    int h, m;
    double s;
    char *str = "";
    h   = (int)(t/3600.);
    if(t < 0.){ t = -t; str = "-";}
    m = (int)((t - (double)h*3600.)/60.);
    s = t - (double)h*3600. - (double)m*60.;
    h %= 24;
    if(s>59) s=59;
    sprintf(buf, "%s%dh:%02dm:%04.1fs", str, h,m,s);
    return buf;
}

char *angle_asc(double a){
    char s;
    int d, min;
    double sec;
    if (a >= 0.)
        s = '+';
    else{
        s = '-'; a = -a;
    }
    d   = (int)(a/3600.);
    min = (int)((a - (double)d*3600.)/60.);
    sec = a - (double)d*3600. - (double)min*60.;
    d %= 360;
    if(sec>59.9) sec=59.9;
    sprintf(buf, "%c%d:%02d':%04.1f''", s,d,min,sec);
    return buf;
}

void write_bta_data(fitsfile *fp){
    char *val;
    double dtmp;
    time_t t_now = time(NULL);
    struct tm *tm_ut;
    tm_ut = gmtime(&t_now);
    if(!shm_ready){
        if(!get_shm_block(&sdat, ClientSide)) return;
        else shm_ready = TRUE;
    }
    if(!check_shm_block(&sdat)) return;
    /*
     * Observatory parameters
     */
    // TELESCOP / Telescope name
    CMNT("Telescope name");
    FTKEY(TSTRING, "TELESCOP", "BTA 6m telescope");
    // placement
    CMNT("Observatory altitude, m");
    dtmp = TELALT; FTKEY(TDOUBLE, "ALT_OBS", &dtmp);
    CMNT("Observatory longitude, degr");
    dtmp = TELLONG; FTKEY(TDOUBLE, "LONG_OBS", &dtmp);
    CMNT("Observatory lattitude, degr");
    dtmp = TELLAT; FTKEY(TDOUBLE, "LAT_OBS", &dtmp);
    /*
     * Times
     */
    dtmp = S_time-EE_time;
    // ST / sidereal time (hh:mm:ss.ms)
    CMNT("Sidereal time: %s", time_asc(dtmp));
    FTKEY(TDOUBLE, "ST", &dtmp);
    // UT / universal time (hh:mm:ss.ms)
    CMNT("Universal time: %s", time_asc(M_time));
    FTKEY(TDOUBLE, "UT", (double*)&M_time);
    CMNT("Julian date");
    FTKEY(TDOUBLE, "JD", (double*)&JDate);
    /*
     * Telescope parameters
     */
    switch(Tel_Focus){
        case Nasmyth1 :  val = "Nasmyth1";  break;
        case Nasmyth2 :  val = "Nasmyth2";  break;
        default       :  val = "Prime";     break;
    }
    // FOCUS / Focus of telescope (mm)
    CMNT("Observation focus");
    FTKEY(TSTRING, "FOCUS", val);
    // VAL_F / focus value
    CMNT("Focus value (mm)");
    FTKEY(TDOUBLE, "VAL_F", (double*)&val_F);
    // EQUINOX / Epoch of RA & DEC
    dtmp = 1900 + tm_ut->tm_year + tm_ut->tm_yday / 365.2422;
    CMNT("Epoch of RA & DEC");
    FTKEY(TDOUBLE, "EQUINOX", &dtmp);
    CMNT("Current object R.A.: %s", time_asc(CurAlpha));
    // RA / Right ascention (H.H)
    dtmp = CurAlpha / 3600.; FTKEY(TDOUBLE, "RA", &dtmp);
    // DEC / Declination (D.D)
    CMNT("Current object Decl.: %s", angle_asc(CurDelta));
    dtmp = CurDelta / 3600.; FTKEY(TDOUBLE, "DEC", &dtmp);
    CMNT("Source R.A.: %s", time_asc(SrcAlpha));
    dtmp = SrcAlpha / 3600.; FTKEY(TDOUBLE, "S_RA", &dtmp);
    CMNT("Source Decl.: %s", angle_asc(SrcDelta));
    dtmp = SrcDelta / 3600.; FTKEY(TDOUBLE, "S_DEC", &dtmp);
    CMNT("Telescope R.A: %s", time_asc(val_Alp));
    dtmp = val_Alp / 3600.; FTKEY(TDOUBLE, "T_RA", &dtmp);
    CMNT("Telescope Decl.: %s", angle_asc(val_Del));
    dtmp = val_Del / 3600.; FTKEY(TDOUBLE, "T_DEC", &dtmp);
    // A / Azimuth
    CMNT("Current object Azimuth: %s", angle_asc(tag_A));
    dtmp = tag_A / 3600.; FTKEY(TDOUBLE, "A", &dtmp);
    // Z / Zenith distance
    CMNT("Current object Zenith: %s", angle_asc(tag_Z));
    dtmp = tag_Z / 3600.; FTKEY(TDOUBLE, "Z", &dtmp);
    // PARANGLE / Parallactic angle
    CMNT("Parallactic  angle: %s", angle_asc(tag_P));
    dtmp = tag_P / 3600.;FTKEY(TDOUBLE, "PARANGLE", &dtmp);

    CMNT("Telescope A: %s", angle_asc(val_A));
    dtmp = val_A / 3600.; FTKEY(TDOUBLE, "VAL_A", &dtmp);
    CMNT("Telescope Z: %s", angle_asc(val_Z));
    dtmp = val_Z / 3600.; FTKEY(TDOUBLE, "VAL_Z", &dtmp);
    CMNT("Current P2 value: %s", angle_asc(val_P));
    dtmp = val_P / 3600.; FTKEY(TDOUBLE, "VAL_P", &dtmp);

    CMNT("Difference A: %s", angle_asc(Diff_A));
    dtmp = Diff_A; FTKEY(TDOUBLE, "DIFF_A", &dtmp);
    CMNT("Difference Z: %s", angle_asc(Diff_Z));
    dtmp = Diff_Z; FTKEY(TDOUBLE, "DIFF_Z", &dtmp);
    CMNT("Difference P2: %s", angle_asc(Diff_P));
    dtmp = Diff_P; FTKEY(TDOUBLE, "DIFF_P", &dtmp);

    CMNT("Dome A: %s", angle_asc(val_D));
    dtmp = val_D / 3600.; FTKEY(TDOUBLE, "VAL_D", &dtmp);
    // OUTTEMP / Out temperature (C)
    CMNT("Outern temperature, degC");
    FTKEY(TDOUBLE, "OUTTEMP", (double*)&val_T1);
    // DOMETEMP / Dome temperature (C)
    CMNT("In-dome temperature, degC");
    FTKEY(TDOUBLE, "DOMETEMP", (double*)&val_T2);
    // MIRRTEMP / Mirror temperature (C)
    CMNT("Mirror temperature, degC");
    FTKEY(TDOUBLE, "MIRRTEMP", (double*)&val_T3);
    // PRESSURE / Pressure (mmHg)
    CMNT("Pressure, mmHg");
    FTKEY(TDOUBLE, "PRESSURE", (double*)&val_B);
    // WIND / Wind speed (m/s)
    CMNT("Wind speed, m/s");
    FTKEY(TDOUBLE, "WIND", (double*)&val_Wnd);
    CMNT("Humidity, %%");
    FTKEY(TDOUBLE, "HUM", (double*)&val_Hmd);
}

#endif // USE_BTA
