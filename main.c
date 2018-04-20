/*
 *                                                                                                  geany_encoding=koi8-r
 * main.c
 *
 * Copyright 2017 Edward V. Emelianov <eddy@sao.ru, edward.emelianoff@gmail.com>
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
 *
 */

// for strcasestr
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <string.h>

#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <fitsio.h>
#include <math.h>
#include <signal.h>
#ifdef USE_BTA
#include "bta_print.h"
#endif
#include "main.h"
#include "atikcore.h"

#ifdef USEPNG
int writepng(char *filename, int width, int height, void *data);
#endif /* USEPNG */

#define BUFF_SIZ 4096

#define TMBUFSIZ 40
char tm_buf[TMBUFSIZ];  // buffer for string with time value

glob_pars *G = NULL; // default parameters see in cmdlnopts.c

uint16_t max = 0, min = 65535; // max/min values for given image
double avr, std; // stat values
double pixX, pixY; // pixel size in um

void print_stat(u_int16_t *img, long size);

size_t curtime(char *s_time){ // current date/time
    time_t tm = time(NULL);
    return strftime(s_time, TMBUFSIZ, "%d/%m/%Y,%H:%M:%S", localtime(&tm));
}

double t_int=1e6;    // CCD temperature @exposition end
struct timeval expStartsAt;     // exposition start time

int check_filename(char *buff, char *outfile, char *ext){
    struct stat filestat;
    int num;
    for(num = 1; num < 10000; num++){
        if(snprintf(buff, BUFF_SIZ, "%s_%04d.%s", outfile, num, ext) < 1)
            return 0;
        if(stat(buff, &filestat)) // no such file or can't stat()
            return 1;
    }
    return 0;
}

void signals(int signo){
    if(signo){
        /// Аварийное завершение с кодом %d
        WARNX(_("Abort with code %d"), signo);
        signal(signo, SIG_DFL);
    }
    DBG("abort exp");
    atik_camera_abortExposure();
    DBG("close");
    atik_camera_close();
    DBG("exit");
    exit(signo);
}

extern const char *__progname;
void info(const char *fmt, ...){
    va_list ar;
    if(!verbose) return;
    green("%s: ", __progname);
    va_start(ar, fmt);
    vprintf(fmt, ar);
    va_end(ar);
    printf("\n");
}

int main(int argc, char **argv){
    int num;
    char *msg = NULL;
    initial_setup();
    signal(SIGTERM, signals); // kill (-15) - quit
    signal(SIGHUP, signals);  // hup - quit
    signal(SIGINT, signals);  // ctrl+C - quit
    signal(SIGQUIT, signals); // ctrl+\ - quit
    signal(SIGTSTP, SIG_IGN); // ignore ctrl+Z

    G = parse_args(argc, argv);
    /*
     * Find CCDs and work with each of them
     */
    num = atik_list_create();
    if(!num) ERRX(_("No CCD found"));
    DBG("List: %s", atik_list_get());
    if(G->camname){
        if(atik_list_item_select(G->camname) == 0)
            ERRX(_("Camera \"%s\" not found"), G->camname);
    }
    if(num > 1 && !G->camname){
        ERRX(_("Found %d cameras, give a specific name with \"--camname\" option"));
    }
    uint16_t *img = NULL;
    DBG("Try to open %s", atik_camera_name());
    if(atik_camera_open() == 0)
        ERRX(_("Can't open camera device"));
    DBG("reset preview, 8bit and dark");
    if(atik_camera_setPreviewMode(0) == 0) WARNX("failed");
    atik_camera_setDarkFrameMode(0);
    atik_camera_set8BitMode(0);
    info("Binlist: %s", atik_camera_getBinList());
    COOLING_STATE state = COOLING_ON;
    float targetTemp, power;
    AtikCapabilities *cap = atik_camera_getCapabilities();
    info("Sensor size: %dx%d pix", cap->pixelCountX, cap->pixelCountY);
    if(G->X1 > (int)cap->pixelCountX || G->X1 < 1) G->X1 = cap->pixelCountX;
    if(G->Y1 > (int)cap->pixelCountY || G->Y1 < 1) G->Y1 = cap->pixelCountY;
    pixX = cap->pixelSizeX, pixY = cap->pixelSizeY;
    info("Pixel size: %gx%g mkm", pixX, pixY);
    info("Max binning: %dx%d", cap->maxBinX, cap->maxBinY);
    if(G->hbin > (int)cap->maxBinX || G->hbin < 1 ||
        G->vbin > (int)cap->maxBinY || G->vbin < 1){
            /// Биннинг должен иметь значение от 1 до %d(H) и %d(V)
            ERRX(_("Binning should have values from 1 to %d(H) and %d(V)"), cap->maxBinX, cap->maxBinY);
        }
    info("Short expositions: min=%gs, max=%gs", cap->minShortExposure, cap->maxShortExposure);
    if(cap->colour != COLOUR_NONE) WARNX(_("Colour camera!"));
    CAMERA_TYPE camtype = atik_camera_getType();
    switch (camtype){
        case ORIGINAL_HSC:
            msg = "ORIGINAL_HSC";
        break;
        case IC24:
            msg = "IC24";
        break;
        case QUICKER:
            msg = "QUICKER";
        break;
        case IIDC:
            msg = "IIDC";
        break;
        case SONY_SCI:
            msg = "SONY_SCI";
        break;
        default:
            msg = "unknown";
    }
    info("Camera type: %s", msg);
    /*{int g, o;
    if(atik_camera_getGain(&g, &o)){
        info("Camera gain: %d, gain offset: %d", g, o);
    }}*/

    if((G->X1 > -1 && G->X1 < G->X0) || (G->Y1 > -1 && G->Y1 < G->Y0)){
        /// X1 и Y1 должны быть больше X0 и Y0
        ERRX(_("X1 and Y1 should be greater than X0 and Y0"));
    }
    if(G->X0 < 0) G->X0 = 0;
    if(G->Y0 < 0) G->Y0 = 0;

    if(G->temperature < 25.){
        // "Установка температуры ПЗС: %g градусов Цельсия\n"
        green(_("Set CCD temperature to %g degr.C\n"), G->temperature);
        if(!atik_camera_setCooling(G->temperature)){
            /// "Ошибка во время установки температуры ПЗС %g"
            WARNX(_("Error when trying to set cooling temperature %g"), G->temperature);
        }
    }

/*
    int j;
    info("%d sensors found", cap->tempSensorCount);
    for(j = 0; j < (int)cap->tempSensorCount; ++j){
        if(atik_camera_getTemperatureSensorStatus(j+1, &targetTemp))
            info("Sensor %d temperature: %.1f", j, targetTemp);
    }
*/
    if(G->shtr_cmd != SHUTTER_LEAVE){
        if(!cap->hasShutter){
            WARNX(_("Selected camera has no shutter"));
        }else{
            shuttercmd shtr = G->shtr_cmd;
            char *str = NULL;
            switch(shtr){
                case SHUTTER_CLOSE:
                    str = _("Close");
                break;
                case SHUTTER_OPEN:
                    str = _("Open");
                break;
                default:
                    ERRX(_("Unknown shutter command"));
            }
            green(_("%s CCD shutter\n"), str);
            if(!atik_camera_setShutter((G->shtr_cmd == SHUTTER_CLOSE) ? 0 : 1)){
                /// "Ошибка изменения состояния затвора"
                WARNX(("Error changing shutter state"));
            }
        }
    }
    if(atik_camera_getCoolingStatus(&state, &targetTemp, &power)){
        switch (state){
            case COOLING_INACTIVE:
                msg = "inactive";
            break;
            case COOLING_ON:
                msg = "cooling on";
            break;
            case COOLING_SETPOINT:
                msg = "cooling to setpoint";
            break;
            case WARMING_UP:
                msg = "warming up";
            break;
            default:
                msg = "unknown";
        }
        info("Cooling status: %s; targetTemp=%.1f, power=%.1f", msg, targetTemp, power);
    }
    if(atik_camera_getTemperatureSensorStatus(1, &targetTemp)){
        info("CCD temperature: %.1f", targetTemp);
    }
    if(G->exptime < 0.) signals(0); // turn off all
    if(G->exptime < cap->minShortExposure) G->exptime = cap->minShortExposure;
    if(G->exptime > cap->maxShortExposure && !cap->supportsLongExposure)
        ERRX(_("This camera doesn't support exposures with length more than %gs"), cap->maxShortExposure);
    info("Exposure time = %gs", G->exptime);
    if(G->dark && !atik_camera_setDarkFrameMode(1)){
        /// "Ошибка: не могу установить режим темновых"
        ERRX(_("Error: can't set dark mode"));
    }
    if(G->fast){
        if(!cap->has8BitMode){
            /// "У данной камеры отсутствует 8-битный режим"
            WARNX(_("This camera has no 8-bit mode"));
        }else if(!atik_camera_set8BitMode(1)){
        /// "Ошибка установки 8-битного режима"
            ERRX(_("Can't set 8-bit mode"));
        }
    }
    if(G->preview && !atik_camera_setPreviewMode(1)){
        /// "Ошибка установки режима предварительного просмотра"
        ERRX(_("Can't set preview mode"));
    }

    long img_rows, row_width, imgSize;
    row_width = atik_camera_imageWidth(G->X1 - G->X0, G->hbin);
    img_rows = atik_camera_imageHeight(G->Y1 - G->Y0, G->vbin);
    imgSize = img_rows * row_width;
    img = MALLOC(uint16_t, imgSize);
    DBG("allocated 2x%ld bytes; X0=%d, X1=%d, Y0=%d, Y1=%d, w=%ld, h=%ld",
        imgSize, G->X0, G->X1, G->Y0, G->Y1, row_width, img_rows);
    int j;

    for (j = 0; j < G->nframes; ++j){
        atik_camera_getTemperatureSensorStatus(1, &targetTemp);
        G->temperature = targetTemp; // temperature @ exp. start
        printf("\n\n");
        /// Захват кадра %d\n
        printf(_("Capture frame %d\n"), j);
        gettimeofday(&expStartsAt, NULL);
        // start exposition & wait
        if(G->exptime < cap->maxShortExposure){ // Short exposure
            if(!atik_camera_readCCD_delay(G->X0, G->Y0, G->X1 - G->X0,
                G->Y1 - G->Y0, G->hbin, G->vbin, G->exptime))
                    ERRX(_("Can't start short exposition!"));
        }else{ // Long exposure
            double time2wait = ((double)atik_camera_delay(G->exptime))/1e6;
            double dtstart = dtime(); // time of exposition start
            if(!atik_camera_startExposure(0))
                ERRX(_("Can't start long exposition!"));
            do{
                atik_camera_getTemperatureSensorStatus(1, &targetTemp);
                t_int = targetTemp;
                if(curtime(tm_buf)){
                    /// дата/время
                    info("%s: %s\tTint=%.2f\n", _("date/time"), tm_buf, t_int);
                }
                else WARNX("curtime() error");
                double t = time2wait - (dtime() - dtstart);
                /// %.3f секунд до окончания экспозиции\n
                printf(_("%.3f seconds till exposition ends\n"), t);
                if(t > 10.) sleep(10);
                else usleep(t*1e6);
            }while(dtime() - dtstart < time2wait);
            if(!atik_camera_readCCD(G->X0, G->Y0, G->X1 - G->X0,
                G->Y1 - G->Y0, G->hbin, G->vbin))
                    ERRX(_("Can't read exposed frame!"));
        }
        info(_("Read image"));
        if(!atik_camera_getImage(img, imgSize))
            ERRX(_("getImage() failed"));
        curtime(tm_buf);
        print_stat(img, row_width * img_rows);
        inline void WRITEIMG(int (*writefn)(char*,int,int,void*), char *ext){
            char buff[BUFF_SIZ], nameok = 0;
            if(rewrite_ifexists){
                char *p = "";
                if(strcmp(ext, "fits") == 0) p = "!";
                if(G->nframes > 1){
                    snprintf(buff, BUFF_SIZ, "%s%s_%04d.%s", p, G->outfile, j, ext);
                }else{
                    snprintf(buff, BUFF_SIZ, "%s%s.%s", p, G->outfile, ext);
                }
                nameok = 1;
            }else{
                if(!check_filename(buff, G->outfile, ext)){
                    /// Не могу сохранить файл
                    WARNX(_("Can't save file"));
                }else{
                    nameok = 1;
                }
            }
            if(nameok){
                if(writefn(buff, row_width, img_rows, img)){
                    /// Не могу записать %s файл
                    WARNX(_("Can't write %s file"), ext);
                }else{
                    /// Файл записан в '%s'\n
                    printf(_("File saved as '%s'\n"), buff);
                }
            }
        }
            #ifdef USERAW
            WRITEIMG(writeraw, "raw");
            #endif // USERAW
            WRITEIMG(writefits, "fits");
            #ifdef USEPNG
            WRITEIMG(writepng, "png");
            #endif // USEPNG
        if(G->pause_len){
            double delta, time1 = dtime() + G->pause_len;
            while((delta = time1 - dtime()) > 0.){
                atik_camera_getTemperatureSensorStatus(1, &targetTemp);
                t_int = targetTemp;
                /// %d секунд до окончания паузы\n
                printf(_("%d seconds till pause ends\n"), (int)delta);
                if(curtime(tm_buf)){
                    /// дата/время
                    info("%s: %s\tTint=%.2f\n", _("date/time"), tm_buf, t_int);
                }
                else info("curtime() error");
                if(delta > 10) sleep(10);
                else sleep((int)delta);
            }
        }
    }
    if(G->warmup) atik_camera_initiateWarmUp();
    atik_camera_close();
    FREE(img);
    atik_list_destroy();
    return 0;
}

#ifdef USERAW
int writeraw(char *filename, int width, int height, void *data){
    int fd, size, err;
    if ((fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC,
        S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH )) == -1){
        WARN("open(%s) failed", filename);
        return -errno;
    }
    size = width * height * sizeof(u_int16_t);
    if ((err = write(fd, data, size)) != size){
        WARN("write() failed");
        err = -errno;
    }
    else err = 0;
    close(fd);
    return err;
}
#endif // USERAW


int writefits(char *filename, int width, int height, void *data){
    long naxes[2] = {width, height}, startTime;
    double tmp = 0.0;
    struct tm *tm_starttime;
    char buf[80];
    time_t savetime = time(NULL);
    fitsfile *fp;
    TRYFITS(fits_create_file, &fp, filename);
    TRYFITS(fits_create_img, fp, USHORT_IMG, 2, naxes);
    // FILE / Input file original name
    WRITEKEY(fp, TSTRING, "FILE", filename, "Input file original name");
    // ORIGIN / organization responsible for the data
    WRITEKEY(fp, TSTRING, "ORIGIN", "SAO RAS", "organization responsible for the data");
    // OBSERVAT / Observatory name
    WRITEKEY(fp, TSTRING, "OBSERVAT", "Special Astrophysical Observatory, Russia", "Observatory name");
    // DETECTOR / detector
    WRITEKEY(fp, TSTRING, "DETECTOR", (char*)atik_camera_name(), "Detector model");
    // INSTRUME / Instrument
    if(G->instrument){
        WRITEKEY(fp, TSTRING, "INSTRUME", G->instrument, "Instrument");
    }else
        WRITEKEY(fp, TSTRING, "INSTRUME", "direct imaging", "Instrument");
    snprintf(buf, 80, "%.g x %.g", pixX, pixY);
    // PXSIZE / pixel size
    WRITEKEY(fp, TSTRING, "PXSIZE", buf, "Approx. pixel size (um)");
    WRITEKEY(fp, TDOUBLE, "XPIXSZ", &pixX, "Pixel Size X (um)");
    WRITEKEY(fp, TDOUBLE, "YPIXSZ", &pixY, "Pixel Size Y (um)");
    // CRVAL1, CRVAL2 / Offset in X, Y
    if(G->X0) WRITEKEY(fp, TINT, "X0", &G->X0, "Subframe left border");
    if(G->Y0) WRITEKEY(fp, TINT, "Y0", &G->Y0, "Subframe upper border");
    if(G->exptime < 2.*DBL_EPSILON) sprintf(buf, "bias");
    else if(G->dark) sprintf(buf, "dark");
    else if(G->objtype) strncpy(buf, G->objtype, 80);
    else sprintf(buf, "object");
    // IMAGETYP / object, flat, dark, bias, scan, eta, neon, push
    WRITEKEY(fp, TSTRING, "IMAGETYP", buf, "Image type");
    // DATAMAX, DATAMIN / Max,min pixel value
    int itmp = 0;
    WRITEKEY(fp, TINT, "DATAMIN", &itmp, "Min pixel value");
    //itmp = G->fast ? 255 : 65535;
    itmp = 65535;
    WRITEKEY(fp, TINT, "DATAMAX", &itmp, "Max pixel value");
    WRITEKEY(fp, TUSHORT, "STATMAX", &max, "Max data value");
    WRITEKEY(fp, TUSHORT, "STATMIN", &min, "Min data value");
    WRITEKEY(fp, TDOUBLE, "STATAVR", &avr, "Average data value");
    WRITEKEY(fp, TDOUBLE, "STATSTD", &std, "Std. of data value");
    WRITEKEY(fp, TDOUBLE, "TEMP0", &G->temperature, "Camera temperature at exp. start (degr C)");
    if(t_int < 100.){
        WRITEKEY(fp, TDOUBLE, "TEMP1", &t_int, "Camera temperature at exp. end (degr C)");
        tmp = (G->temperature + t_int) / 2. + 273.15;
    }else tmp = G->temperature + 273.15;
    // CAMTEMP / Camera temperature (K)
    WRITEKEY(fp, TDOUBLE, "CAMTEMP", &tmp, "Average camera temperature (K)");
    tmp = (double)G->exptime;
    // EXPTIME / actual exposition time (sec)
    WRITEKEY(fp, TDOUBLE, "EXPTIME", &tmp, "Actual exposition time (sec)");
    // DATE / Creation date (YYYY-MM-DDThh:mm:ss, UTC)
    strftime(buf, 80, "%Y-%m-%dT%H:%M:%S", gmtime(&savetime));
    WRITEKEY(fp, TSTRING, "DATE", buf, "Creation date (YYYY-MM-DDThh:mm:ss, UTC)");
    startTime = (long)expStartsAt.tv_sec;
    tm_starttime = localtime(&expStartsAt.tv_sec);
    strftime(buf, 80, "exposition starts at %d/%m/%Y, %H:%M:%S (local)", tm_starttime);
    tmp = startTime + (double)expStartsAt.tv_usec/1e6;
    WRITEKEY(fp, TDOUBLE, "UNIXTIME", &tmp, buf);
    strftime(buf, 80, "%Y/%m/%d", tm_starttime);
    // DATE-OBS / DATE (YYYY/MM/DD) OF OBS.
    WRITEKEY(fp, TSTRING, "DATE-OBS", buf, "DATE OF OBS. (YYYY/MM/DD, local)");
    strftime(buf, 80, "%H:%M:%S", tm_starttime);
    // START / Measurement start time (local) (hh:mm:ss)
    WRITEKEY(fp, TSTRING, "START", buf, "Measurement start time (hh:mm:ss, local)");
    // OBJECT  / Object name
    if(G->objname){
        WRITEKEY(fp, TSTRING, "OBJECT", G->objname, "Object name");
    }
    // BINNING / Binning
    if(G->hbin != 1 || G->vbin != 1){
        snprintf(buf, 80, "%d x %d", G->hbin, G->vbin);
        WRITEKEY(fp, TSTRING, "BINNING", buf, "Binning (hbin x vbin)");
    }
    // OBSERVER / Observers
    if(G->observers){
        WRITEKEY(fp, TSTRING, "OBSERVER", G->observers, "Observers");
    }
    // PROG-ID / Observation program identifier
    if(G->prog_id){
        WRITEKEY(fp, TSTRING, "PROG-ID", G->prog_id, "Observation program identifier");
    }
    // AUTHOR / Author of the program
    if(G->author){
        WRITEKEY(fp, TSTRING, "AUTHOR", G->author, "Author of the program");
    }
    #ifdef USE_BTA
    write_bta_data(fp);
    #endif
    TRYFITS(fits_write_img, fp, TUSHORT, 1, width * height, data);
    TRYFITS(fits_close_file, fp);
    return 0;
}

#ifdef USEPNG
int writepng(char *filename, int width, int height, void *data){
    int err;
    FILE *fp = NULL;
    png_structp pngptr = NULL;
    png_infop infoptr = NULL;
    void *row;
    if ((fp = fopen(filename, "wb")) == NULL){
        err = -errno;
        goto done;
    }
    if ((pngptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                            NULL, NULL, NULL)) == NULL){
        err = -ENOMEM;
        goto done;
    }
    if ((infoptr = png_create_info_struct(pngptr)) == NULL){
        err = -ENOMEM;
        goto done;
    }
    png_init_io(pngptr, fp);
    png_set_compression_level(pngptr, Z_BEST_COMPRESSION);
    png_set_IHDR(pngptr, infoptr, width, height, 16, PNG_COLOR_TYPE_GRAY,
                PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                PNG_FILTER_TYPE_DEFAULT);
    png_write_info(pngptr, infoptr);
    png_set_swap(pngptr);
    for(row = data; height > 0; row += width * sizeof(u_int16_t), height--)
        png_write_row(pngptr, row);
    png_write_end(pngptr, infoptr);
    err = 0;
    done:
    if(fp) fclose(fp);
    if(pngptr) png_destroy_write_struct(&pngptr, &infoptr);
    return err;
}
#endif /* USEPNG */

void print_stat(u_int16_t *img, long size){
    long i, Noverld = 0L;
    double pv, sum=0., sum2=0., sz = (double)size;
    u_int16_t *ptr = img, val;
    max = 0; min = 65535;
    for(i = 0; i < size; i++, ptr++){
        val = *ptr;
        pv = (double) val;
        sum += pv;
        sum2 += (pv * pv);
        if(max < val) max = val;
        if(min > val) min = val;
        if(val >= 65530) Noverld++;
    }
    // Статистика по изображению:\n
    printf(_("Image stat:\n"));
    avr = sum/sz;
    printf("avr = %.1f, std = %.1f, Noverload = %ld\n", avr,
        std = sqrt(fabs(sum2/sz - avr*avr)), Noverld);
    printf("max = %u, min = %u, size = %ld\n", max, min, size);
}
