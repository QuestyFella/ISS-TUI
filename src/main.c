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

#define TRAIL 1024
#define LAND_W 180
#define LAND_H 60

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

static int fetch_iss(struct iss *iss, char *error, size_t error_size) {
#ifdef _WIN32
    snprintf(error, error_size, "live tracking needs curl/popen; Windows is not supported in this tiny build");
    return 0;
#else
    FILE *fp = popen("(curl -fsS --connect-timeout 2 --max-time 5 https://api.wheretheiss.at/v1/satellites/25544 || curl -fsS --connect-timeout 2 --max-time 5 http://api.open-notify.org/iss-now.json) 2>/dev/null", "r");
    char json[4096] = {0};
    size_t used = 0;

    if (fp == NULL) {
        snprintf(error, error_size, "could not run curl");
        return 0;
    }

    while (used + 1 < sizeof(json) && fgets(json + used, (int)(sizeof(json) - used), fp) != NULL) {
        used = strlen(json);
    }

    if (pclose(fp) != 0 || used == 0) {
        snprintf(error, error_size, "curl failed or returned no data");
        return 0;
    }

    if (!json_number(json, "\"latitude\"", &iss->lat) ||
        !json_number(json, "\"longitude\"", &iss->lon) ||
        !json_number(json, "\"timestamp\"", &iss->timestamp)) {
        snprintf(error, error_size, "could not parse ISS JSON");
        return 0;
    }

    if (!json_number(json, "\"altitude\"", &iss->altitude)) {
        iss->altitude = -1.0;
    }
    if (!json_number(json, "\"velocity\"", &iss->velocity)) {
        iss->velocity = -1.0;
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
        *height = (int)ws.ws_row - 5;
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

static void draw(const struct iss *iss, int has_fix, const struct track trail[], int trail_count, const char *status) {
    int width = 0;
    int height = 0;
    char *map = NULL;
    unsigned char *dots = NULL;
    int x = 0;
    int y = 0;

    map_size(&width, &height);
    map = malloc((size_t)width * (size_t)height);
    dots = malloc((size_t)width * (size_t)height);
    if (map == NULL || dots == NULL) {
        free(map);
        free(dots);
        return;
    }

    memset(map, ' ', (size_t)width * (size_t)height);
    memset(dots, 0, (size_t)width * (size_t)height);

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

    for (int i = 0; i < trail_count; i++) {
        project(trail[i].lat, trail[i].lon, width, height, &x, &y);
        int age = trail_count - 1 - i;
        char c;
        if (age < 2) {
            c = '*';
        } else if (age < 6) {
            c = '+';
        } else if (age < 14) {
            c = ':';
        } else {
            c = '.';
        }
        map[y * width + x] = c;
    }

    if (has_fix) {
        project(iss->lat, iss->lon, width, height, &x, &y);
        map[y * width + x] = '@';
    }

    printf("\033[H\033[2J");
    printf("ISS tracker - Ctrl-C to quit\n");
    if (has_fix) {
        printf("lat %.3f  lon %.3f  ", iss->lat, iss->lon);
        if (iss->altitude >= 0.0) {
            printf("alt %.0f km  ", iss->altitude);
        } else {
            printf("alt unknown  ");
        }
        if (iss->velocity >= 0.0) {
            printf("velocity %.0f km/h  ", iss->velocity);
        } else {
            printf("velocity unknown  ");
        }
        printf("%.0f\n", iss->timestamp);
    } else {
        printf("waiting for first fix\n");
    }
    printf("status: %s\n", status);

    print_rule('+', '-', '+', width);
    for (int row = 0; row < height; row++) {
        putchar('|');
        for (int col = 0; col < width; col++) {
            char cell = map[row * width + col];
            if (cell == '@') {
                printf("\033[1;38;2;255;30;30;48;2;255;200;50m@\033[0m");
            } else if (cell == '*') {
                printf("\033[1;38;2;255;180;50m*\033[0m");
            } else if (cell == '+') {
                printf("\033[38;2;200;160;40m+\033[0m");
            } else if (cell == ':') {
                printf("\033[38;2;120;100;50m:\033[0m");
            } else if (cell == '.') {
                printf("\033[38;2;60;60;60m.\033[0m");
            } else {
                unsigned char pattern = dots[row * width + col];
                if (pattern != 0) {
                    printf("\033[38;2;140;125;100m");
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
    free(map);
    free(dots);
}

static void sleep_one_second(void) {
    struct timespec ts = {1, 0};
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR && running) {
    }
}

int main(void) {
    struct iss iss = {0};
    struct track trail[TRAIL];
    int trail_count = 0;
    int has_fix = 0;
    char status[128] = "starting";

    if (!isatty(STDOUT_FILENO)) {
        puts(iss_tui_name());
        return 0;
    }

    signal(SIGINT, stop);

    while (running) {
        if (fetch_iss(&iss, status, sizeof(status))) {
            strcpy(status, "live");
            has_fix = 1;
        }

        if (has_fix) {
            if (trail_count < TRAIL) {
                trail[trail_count].lat = iss.lat;
                trail[trail_count].lon = iss.lon;
                trail_count++;
            } else {
                memmove(trail, trail + 1, sizeof(trail[0]) * (TRAIL - 1));
                trail[TRAIL - 1].lat = iss.lat;
                trail[TRAIL - 1].lon = iss.lon;
            }
        }

        draw(&iss, has_fix, trail, trail_count, status);
        sleep_one_second();
    }

    printf("\033[0m\n");
    return 0;
}
