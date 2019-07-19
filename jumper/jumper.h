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

struct _pedal_t
{
    /* function table */
    void (* reset)(struct _pedal_t * this);
    void (* rise)(struct _pedal_t * this);
	void (* erase)(struct _pedal_t * this);
	void (* erase_slice)(struct _pedal_t * this, int x);
	void (* draw)(struct _pedal_t * this);
	void (* draw_slice)(struct _pedal_t * this, int x);
    /* data fields */
    int x;
    int y;
    int length;
    char type;
    struct _actor_t * actor;
    /* sibing list */
    DECLARE_LISTHEAD(sibling);
};
typedef struct _pedal_t pedal_t;

struct _actor_t
{
    /* function table */
    int (* move_left)(struct _actor_t * this);
    int (* move_right)(struct _actor_t * this);
	int (* move_up)(struct _actor_t * this);
	int (* move_down)(struct _actor_t * this);
	void (* erase)(struct _actor_t * this);
	void (* draw)(struct _actor_t * this);
    /* data fields */
    int x;
    int y;
    int lives;
	int initialized;
    /* pedal standing on */
	pedal_t * pedal;
};
typedef struct _actor_t actor_t;

#endif
