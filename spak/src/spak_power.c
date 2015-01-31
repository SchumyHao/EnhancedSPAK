/*
 * Copyright (c) 2002 University of Utah and the Flux Group.
 * All rights reserved.
 *
 * This file is part of SPAK.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation is hereby granted without fee, provided that the
 * above copyright notice and this permission/disclaimer notice is
 * retained in all copies or modified versions, and that both notices
 * appear in supporting documentation.  THE COPYRIGHT HOLDERS PROVIDE
 * THIS SOFTWARE "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  THE COPYRIGHT
 * HOLDERS DISCLAIM ANY LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER
 * RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Users are requested, but not required, to send to csl-dist@cs.utah.edu
 * any improvements that they make and grant redistribution rights to the
 * University of Utah.
 *
 * Author: John Regehr (regehr@cs.utah.edu)
 */

#include "spak_public.h"
#include "spak_internal.h"

#define DBG_LEVEL 3

static energy_value calculate_tast_engergy(struct task* t){
	assert(t);

	return (energy_value)((double)(t->C) * pow(valid_f_scale[t->f], 3.0));
}

power_value calculate_tast_set_average_power(struct task_set* ts){
	assert(ts);
	assert(ts->num_tasks != 0);

	int i = 0;
	power_value p = 0;

	if(feasible(ts, TRUE)!=ts->num_tasks){
		return -1;
	}

	energy_value sum = 0;
	for(i=0; i<ts->num_tasks; i++){
		sum += calculate_tast_engergy(&ts->tasks[i]);
	}

	p = (power_value)(sum);

	return p;
}

