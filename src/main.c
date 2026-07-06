#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#ifndef _WIN32
#include <sys/ioctl.h>
#endif

#include "iss_tui.h"

#define MAX_SATS 3
#define TRAIL 256
#define LAND_W 180
#define LAND_H 60

/* ponytail: overlay packs trail as sat*4+age+1 (1-12) and markers as sat+13 (13-15).
   This breaks at MAX_SATS>3 — sat 3 trail collides with sat 0 marker. */
_Static_assert(MAX_SATS <= 3, "overlay encoding overflows at MAX_SATS>3");

struct iss {
    double lat;
    double lon;
    double altitude;
    double velocity;
    double timestamp;
};

struct track {
    double lat;
    double lon;
};

struct sat {
    int norad_id;
    const char *name;
    char marker;
    int fg[3];
    struct iss pos;
    int has_fix;
    struct track trail[TRAIL];
    int trail_count;
};

/* Natural Earth 110m land, rasterized once into an equirectangular mask. */
static const char *LAND[LAND_H] = {
    "",
    "",
    "                                               # ##########        ############",
    "                                          ###########    ########################              ##### ##                                  ###",
    "                             ##    #    # ########     ########################                                           ##                ######",
    "                            ####     # ## ##  # # #           ##################                                     ##           ###################   ##      ##",
    "          ####                   ######   ##  ##########       ##############                         ####                 ### ###########################################",
    "##      ########################### ########## ##  #  ####     ##########                        #############  # ############ #####################################################",
    "   #   ####################################### ##  ######       ######         ####            ###### ##### #######################################################################",
    "       ####################################        ###           ####                        ######  #################################################################### # #####",
    "          ###        ######################        ##### ##                            #     #  ##    ###########################################################        ##",
    "         #              #######################    #########                            #     ###    #########################################################          ###",
    "                          #######################  ###########                       ## ### ######################################################################      #",
    "                           #############################  #  #                            ###################################################################### #",
    "                            ##############################                               ################ ## ######  ##########################################",
    "                            ###########################                               ## ##### ## ######      ####  #########################################    ##",
    "                            #########################                                 ####    #  ## ##  ########### ################################## ####     #",
    "                             #######################                                  ###       ##   #  ###########  ################################ #  ##    #",
    "                              #####################                                    ########             ##########################################   #  # #",
    "                                #################                                    #############  ###  # ############################################    #",
    "                                  ########       #                                  ###################### #######  ###################################",
    "                                   ######        #                                 ######################## ########    ##############################",
    "                                     ####        #                                ##########################  ##########     ########## ###########   #",
    "            #                        #####   #        #                           ########################### #########       #######    ###### #",
    "                                         #####                                    ############################ #####           ####      # #####      #",
    "                                             ###                                  ############################# ##             ###         ######      #",
    "                                               #    ##### ##                       ################################             ##         #  ##       ##",
    "                                                   ##########                       ###############################               #                    ##",
    "                                                   #############                             #####################                        # ##     ##",
    "                                                   ##############                              #################                           ##    ####   #",
    "                                                  #################                            ################                             ##   ###  #     #",
    "                                                 ######################                         ##############                                #        #      #####   #",
    "                                                  #######################                       ##############                                  ##             #####",
    "                                                   #####################                         #############",
    "                                                    ###################                         ##############    #                                        ###   #",
    "                                                     #################                          ##############  ###                                    ########  ##                #",
    "                                                       ###############                          ############    ##                                     #############",
    "                                                       ##############                            ###########    ##                                 ##################",
    "                                                       ###########                               #########                                         ###################",
    "                                                      ############                                ########                                         ####################",
    "                                                      ##########                                   ######                                           ##################",
    "                                                      ####### #                                                                                     #          ######           #",
    "                                                     ########                                                                                                   #####            #",
    "                                                     ######",
    "                                                      ###                                                                                                          #           #",
    "                                                    ####                                                                                                                      #",
    "                                                    ####                                                                    #",
    "                                                     ##",
    "                                                       #",
    "",
    "",
    "",
    "                                                        #                                                         ######           ################################",
    "                                                      #####                                  ## ##### ######################   ############################################",
    "                          #           ### ######  #  #######                      #############################################################################################",
    "                 ######################################                     ################################################################################################",
    "         #      ####################################             ###       ################################################################################################",
    "              ##############################################   #############################################################################################################",
    "############### ####################################################################################################################################################################",
    "####################################################################################################################################################################################",
};

static volatile sig_atomic_t running = 1;

static void stop(int sig) {
    (void)sig;
    running = 0;
}

static int json_number(const char *json, const char *key, double *out) {
    const char *p = strstr(json, key);
    char *end = NULL;
    if (p == NULL) {
        return 0;
    }

    p = strchr(p, ':');
    if (p == NULL) {
        return 0;
    }

    errno = 0;
    p++;
    while (*p == ' ' || *p == '\t' || *p == '"') {
        p++;
    }

    *out = strtod(p, &end);
    return errno == 0 && end != p;
}

static int fetch_sat(struct sat *sat, char *error, size_t error_size) {
#ifdef _WIN32
    snprintf(error, error_size, "%s: Windows not supported", sat->name);
    return 0;
#else
    char cmd[384];
    if (sat->norad_id == 25544) {
        /* ISS: primary wheretheiss.at, fallback open-notify */
        snprintf(cmd, sizeof(cmd),
            "(curl -fsS --connect-timeout 2 --max-time 5 "
            "https://api.wheretheiss.at/v1/satellites/25544 || "
            "curl -fsS --connect-timeout 2 --max-time 5 "
            "http://api.open-notify.org/iss-now.json) 2>/dev/null");
    } else {
        /* Non-ISS: Satlas (wheretheiss.at only supports ISS 25544) */
        snprintf(cmd, sizeof(cmd),
            "curl -fsS --connect-timeout 2 --max-time 5 "
            "https://satlas.app/api/satellite-info?query=%d 2>/dev/null",
            sat->norad_id);
    }

    FILE *fp = popen(cmd, "r");
    char json[4096] = {0};
    size_t used = 0;

    if (fp == NULL) {
        snprintf(error, error_size, "%s: could not run curl", sat->name);
        return 0;
    }

    while (used + 1 < sizeof(json) && fgets(json + used, (int)(sizeof(json) - used), fp) != NULL) {
        used += strlen(json + used);
    }

    if (pclose(fp) != 0 || used == 0) {
        snprintf(error, error_size, "%s: fetch failed", sat->name);
        return 0;
    }

    if (sat->norad_id == 25544) {
        /* ISS: wheretheiss.at / open-notify keys */
        if (!json_number(json, "\"latitude\"", &sat->pos.lat) ||
            !json_number(json, "\"longitude\"", &sat->pos.lon) ||
            !json_number(json, "\"timestamp\"", &sat->pos.timestamp)) {
            snprintf(error, error_size, "%s: parse failed", sat->name);
            return 0;
        }
        if (!json_number(json, "\"altitude\"", &sat->pos.altitude)) {
            sat->pos.altitude = -1.0;
        }
        if (!json_number(json, "\"velocity\"", &sat->pos.velocity)) {
            sat->pos.velocity = -1.0;
        }
    } else {
        /* Satlas: no timestamp returned, use time(NULL); velocity in kmps -> km/h */
        if (!json_number(json, "\"latitude\"", &sat->pos.lat) ||
            !json_number(json, "\"longitude\"", &sat->pos.lon)) {
            snprintf(error, error_size, "%s: parse failed", sat->name);
            return 0;
        }
        sat->pos.timestamp = (double)time(NULL);
        if (!json_number(json, "\"altitude_km\"", &sat->pos.altitude)) {
            sat->pos.altitude = -1.0;
        }
        {
            double v = 0.0;
            if (json_number(json, "\"velocity_kmps\"", &v)) {
                sat->pos.velocity = v * 3600.0;
            } else {
                sat->pos.velocity = -1.0;
            }
        }
    }

    return 1;
#endif
}

static void project(double lat, double lon, int width, int height, int *x, int *y) {
    *x = (int)((lon + 180.0) * (width - 1) / 360.0 + 0.5);
    *y = (int)((90.0 - lat) * (height - 1) / 180.0 + 0.5);

    if (*x < 0) *x = 0;
    if (*x >= width) *x = width - 1;
    if (*y < 0) *y = 0;
    if (*y >= height) *y = height - 1;
}

static void map_size(int *width, int *height) {
    *width = 100;
    *height = 32;
#ifndef _WIN32
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0) {
        *width = (int)ws.ws_col - 2;
        *height = (int)ws.ws_row - (4 + MAX_SATS);
    }
#endif
    if (*width < 40) *width = 40;
    if (*height < 12) *height = 12;
    if (*width > LAND_W) *width = LAND_W;
    if (*height > LAND_H) *height = LAND_H;
}

static int sample_mask(int sub_row, int sub_col, int width, int height) {
    int y = sub_row * LAND_H / (height * 4);
    int x = sub_col * LAND_W / (width * 2);
    if (y < 0 || y >= LAND_H || x < 0 || x >= LAND_W) {
        return 0;
    }
    size_t len = strlen(LAND[y]);
    return (size_t)x < len && LAND[y][x] != ' ';
}

static void put_braille(int pattern) {
    int offset = pattern & 0xFF;
    putchar(0xE2);
    putchar(0xA0 + (offset >> 6));
    putchar(0x80 + (offset & 0x3F));
}

static void print_rule(char left, char mid, char right, int width) {
    putchar(left);
    for (int i = 0; i < width; i++) {
        putchar(mid);
    }
    puts((char[]){right, '\0'});
}

static void draw(struct sat sats[], int nsats, const char *status) {
    int width = 0;
    int height = 0;
    unsigned char *dots = NULL;
    unsigned char *overlay = NULL;

    map_size(&width, &height);
    dots = malloc((size_t)width * (size_t)height);
    overlay = malloc((size_t)width * (size_t)height);
    if (dots == NULL || overlay == NULL) {
        free(dots);
        free(overlay);
        return;
    }

    memset(dots, 0, (size_t)width * (size_t)height);
    memset(overlay, 0, (size_t)width * (size_t)height);

    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {
            unsigned char pattern = 0;
            for (int dr = 0; dr < 4; dr++) {
                for (int dc = 0; dc < 2; dc++) {
                    int sr = row * 4 + dr;
                    int sc = col * 2 + dc;
                    if (sample_mask(sr, sc, width, height)) {
                        if (dc == 0) {
                            pattern |= (unsigned char)((dr == 3) ? 0x40 : (1U << dr));
                        } else {
                            pattern |= (unsigned char)((dr == 3) ? 0x80 : (1U << (dr + 3)));
                        }
                    }
                }
            }
            dots[row * width + col] = pattern;
        }
    }

    /* Trails: overlay = sat*4 + age_bracket + 1 (1-12) */
    for (int s = 0; s < nsats; s++) {
        if (!sats[s].has_fix) {
            continue;
        }
        for (int i = 0; i < sats[s].trail_count; i++) {
            int x = 0;
            int y = 0;
            project(sats[s].trail[i].lat, sats[s].trail[i].lon, width, height, &x, &y);
            int age = sats[s].trail_count - 1 - i;
            int age_bracket = (age < 2) ? 0 : (age < 6) ? 1 : (age < 14) ? 2 : 3;
            overlay[y * width + x] = (unsigned char)(s * 4 + age_bracket + 1);
        }
    }

    /* Markers: overlay = sat + 13 (13-15) */
    for (int s = 0; s < nsats; s++) {
        if (!sats[s].has_fix) {
            continue;
        }
        int x = 0;
        int y = 0;
        project(sats[s].pos.lat, sats[s].pos.lon, width, height, &x, &y);
        overlay[y * width + x] = (unsigned char)(s + 13);
    }

    printf("\033[H\033[2J");
    printf("Satellite tracker - Ctrl-C to quit\n");
    for (int s = 0; s < nsats; s++) {
        printf("\033[38;2;%d;%d;%dm%-8s\033[0m ",
               sats[s].fg[0], sats[s].fg[1], sats[s].fg[2], sats[s].name);
        if (sats[s].has_fix) {
            printf("lat %.1f lon %.1f", sats[s].pos.lat, sats[s].pos.lon);
            if (sats[s].pos.altitude >= 0.0) {
                printf(" alt %.0fkm", sats[s].pos.altitude);
            }
            if (sats[s].pos.velocity >= 0.0) {
                printf(" vel %.0fkm/h", sats[s].pos.velocity);
            }
        } else {
            printf("waiting...");
        }
        printf("\n");
    }
    printf("status: %s\n", status);

    print_rule('+', '-', '+', width);
    for (int row = 0; row < height; row++) {
        putchar('|');
        for (int col = 0; col < width; col++) {
            unsigned char cell = overlay[row * width + col];
            if (cell >= 13) {
                int s = cell - 13;
                printf("\033[1;38;2;%d;%d;%dm%c\033[0m",
                       sats[s].fg[0], sats[s].fg[1], sats[s].fg[2], sats[s].marker);
            } else if (cell >= 1) {
                int s = (cell - 1) / 4;
                int age = (cell - 1) % 4;
                int dim = age * 50;
                int r = sats[s].fg[0] - dim;
                int g = sats[s].fg[1] - dim;
                int b = sats[s].fg[2] - dim;
                if (r < 20) r = 20;
                if (g < 20) g = 20;
                if (b < 20) b = 20;
                unsigned char pattern = dots[row * width + col];
                if (pattern != 0) {
                    printf("\033[38;2;%d;%d;%dm", r, g, b);
                    put_braille(pattern);
                    printf("\033[0m");
                } else {
                    char tc;
                    if (age == 0) {
                        tc = '*';
                    } else if (age == 1) {
                        tc = '+';
                    } else if (age == 2) {
                        tc = ':';
                    } else {
                        tc = '.';
                    }
                    printf("\033[38;2;%d;%d;%dm%c\033[0m", r, g, b, tc);
                }
            } else {
                unsigned char pattern = dots[row * width + col];
                if (pattern != 0) {
                    printf("\033[38;2;50;205;50m");
                    put_braille(pattern);
                    printf("\033[0m");
                } else {
                    putchar(' ');
                }
            }
        }
        putchar('|');
        putchar('\n');
    }
    print_rule('+', '-', '+', width);

    fflush(stdout);
    free(dots);
    free(overlay);
}

static void sleep_one_second(void) {
    struct timespec ts = {1, 0};
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR && running) {
    }
}

int main(void) {
    struct sat sats[MAX_SATS] = {
        {.norad_id = 25544, .name = "ISS",      .marker = '@', .fg = {255, 30, 30}},
        {.norad_id = 48274, .name = "Tiangong", .marker = 'T', .fg = {50, 255, 200}},
        {.norad_id = 20580, .name = "Hubble",   .marker = 'H', .fg = {255, 50, 255}},
    };
    char status[128] = "starting";
    int fetch_idx = 0;

    if (!isatty(STDOUT_FILENO)) {
        puts(iss_tui_name());
        return 0;
    }

    signal(SIGINT, stop);

    while (running) {
        if (fetch_sat(&sats[fetch_idx], status, sizeof(status))) {
            snprintf(status, sizeof(status), "%s: live", sats[fetch_idx].name);
            sats[fetch_idx].has_fix = 1;

            struct sat *s = &sats[fetch_idx];
            if (s->trail_count < TRAIL) {
                s->trail[s->trail_count].lat = s->pos.lat;
                s->trail[s->trail_count].lon = s->pos.lon;
                s->trail_count++;
            } else {
                memmove(s->trail, s->trail + 1, sizeof(s->trail[0]) * (TRAIL - 1));
                s->trail[TRAIL - 1].lat = s->pos.lat;
                s->trail[TRAIL - 1].lon = s->pos.lon;
            }
        }

        draw(sats, MAX_SATS, status);
        fetch_idx = (fetch_idx + 1) % MAX_SATS;
        sleep_one_second();
    }

    printf("\033[0m\n");
    return 0;
}
