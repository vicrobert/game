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
#ifndef __LIST_HD_INCL
#define __LIST_HD_INCL

/* List realization */
struct _listhead_t {
	struct _listhead_t *prev;
	struct _listhead_t *next;
};
typedef struct _listhead_t listhead_t;

#define DECLARE_LISTHEAD(lh_var_name)	listhead_t lh_var_name
#define listhead(obj_ptr, lh_var_name)	((obj_ptr)? (&(obj_ptr)->lh_var_name): NULL)
#define member_offset(type, member)		(unsigned long)&((type *)0)->member
#define list_entry(type, ptr, member)	\
	((ptr)? ((type *)(((unsigned long)(ptr)) - member_offset(type, member))): NULL)

/* The following `ptr` according to the pointer of listhead_t */
#define _list_insert_(headptr, ptr) \
	do { \
		(ptr)->next = (headptr); \
		if (headptr) { \
			(ptr)->prev = (headptr)->prev; \
			if ((headptr)->prev) (headptr)->prev->next = (ptr); \
			(headptr)->prev = (ptr); \
		} else (ptr)->prev = NULL; \
	} while (0)

#define _list_append_(tailptr, ptr) \
	do { \
		(ptr)->prev = (tailptr); \
		if (tailptr) { \
    		(ptr)->next = (tailptr)->next; \
    		if ((tailptr)->next) (tailptr)->next->prev = (ptr); \
    		(tailptr)->next = (ptr); \
    	} else (ptr)->next = NULL; \
	} while (0)

#define _list_remove_(ptr) \
	do { \
		if ((ptr)->prev) \
			(ptr)->prev->next = (ptr)->next; \
		if ((ptr)->next) \
			(ptr)->next->prev = (ptr)->prev; \
		(ptr)->prev = (ptr)->next = NULL; \
	} while (0)

#define list_init_listhead(obj_ptr, lh_var_name) \
	listhead(obj_ptr, lh_var_name)->prev = NULL, listhead(obj_ptr, lh_var_name)->next = NULL;

#define list_insert_before(obj_ptr, to_ptr, lh_var_name) \
	_list_insert_(listhead(to_ptr, lh_var_name), listhead(obj_ptr, lh_var_name))

#define list_append(obj_ptr, to_ptr, lh_var_name) \
	_list_append_(listhead(to_ptr, lh_var_name), listhead(obj_ptr, lh_var_name))

#define list_remove(obj_ptr, lh_var_name) \
	_list_remove_(listhead(obj_ptr, lh_var_name))

#define list_prev_obj(obj_ptr, obj_type, lh_var_name) \
	list_entry(obj_type, listhead(obj_ptr, lh_var_name)->prev, lh_var_name)

#define list_next_obj(obj_ptr, obj_type, lh_var_name) \
	list_entry(obj_type, listhead(obj_ptr, lh_var_name)->next, lh_var_name)

#define list_tail_obj(head_obj_ptr, obj_type, lh_var_name, tail_obj_ptr)	\
	do { \
		obj_type *obj_ptr; \
		for (obj_ptr = head_obj_ptr; obj_ptr != NULL; \
			tail_obj_ptr = obj_ptr, obj_ptr = list_next_obj(obj_ptr, obj_type, lh_var_name)); \
	} while (0)

#define list_for_each(obj_ptr, head_obj_ptr, obj_type, lh_var_name) \
	for (obj_ptr = head_obj_ptr; obj_ptr != NULL; \
		obj_ptr = list_next_obj(obj_ptr, obj_type, lh_var_name))

#define list_for_each_reverse(obj_ptr, tail_obj_ptr, obj_type, lh_var_name) \
	for (obj_ptr = tail_obj_ptr; obj_ptr != NULL; \
		obj_ptr = list_prev_obj(obj_ptr, obj_type, lh_var_name))

#endif
