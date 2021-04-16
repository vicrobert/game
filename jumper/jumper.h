/*
 * list.h: Header File of the list
 *
 * (C) Copyright 2017 Reserved By Junbo Yang(Robert.Yeoh)
 * Author: Junbo Yang<mailto:junbo.yang@i-soft.com.cn>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
*/
#ifndef __JUMPER_HD_INCL
#define __JUMPER_HD_INCL
#include "list.h"

#define WIDTH     30
#define HEIGHT    30
#define ORG_X     1
#define ORG_Y     1
#define DIRECT_UP       1
#define DIRECT_DOWN     2
#define DIRECT_RIGHT    3
#define DIRECT_LEFT     4
#define GAME_STAT_STOP          0
#define GAME_STAT_READY         1
#define GAME_STAT_PLAYING       2
#define GAME_STAT_PAUSE         3

/* 0 < GAME_SPEED_ACC_FACT <= 10.0 */
#define GAME_SPEED_ACC_FACT     10.0

#define PEDAL_POOL_MAX  10

/* suggest >=3 and <= 5 */
#define PEDAL_LENGTH_MIN    5

/* PEDAL_LENGTH_MIN <= PEDAL_LENGTH_MAX <= WIDTH - 2 */
#define PEDAL_LENGTH_MAX    10

/* result code for hit test */
#define HITR_NOCRASH      0
#define HITR_PEDAL        1
#define HITR_TOP_BOUNDARY 2
#define HITR_BOT_BOUNDARY 3
#define HITR_TRAP         4

/* actor lives */
#define ACTOR_LIVES 3

#define print_s(s, x, y) \
    printf("\033[%d;%dH%s\n", (y) + ORG_Y, (x) + ORG_X, (s))

#define print_c(c, x, y) \
    printf("\033[%d;%dH%c\n", (y) + ORG_Y, (x) + ORG_X, (c))
    
#define print_i(i, x, y) \
    printf("\033[%d;%dH%d\n", (y) + ORG_Y, (x) + ORG_X, (i))

enum _pedal_type_t
{
    PEDAL_TYPE_NOR,
    PEDAL_TYPE_TRAP
};
typedef enum _pedal_type_t pedal_type_t;

struct _pedal_t
{
    int x;
    int y;
    int length;
    int blood_x;
    pedal_type_t type;
    struct _actor_t * actor;
    /* sibing list */
    DECLARE_LISTHEAD(sibling);
};
typedef struct _pedal_t pedal_t;

struct _actor_t
{
    int x;
    int y;
    int lives;
    int initialized;
    /* pedal standing on */
    pedal_t * pedal;
};
typedef struct _actor_t actor_t;

void initialize();
void uninitialize();
void game_ready();
int game_play();
void game_reset();
int game_exit(int exit_code);
int score(int clean);
void show_lives(int lives);

#endif
