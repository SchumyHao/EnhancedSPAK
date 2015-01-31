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

#define DBG_LEVEL 1
#define UTILIZATION_ACCURACY  (0.01)

struct task_set* create_random_task_set (int num,
        int total_tasks,
        int max_deadline,
        const char* analysis,
        int multiplier,
        int has_jitter,
        int has_independent_deadline)
{
    struct task_set* ts;
    time_value C, T, D, J;
    double U, Umin, Umax;
    int i, j;
    char name[30];
    time_value dls[1000];

    Umin = 0.1 / total_tasks;
    Umax = 2.0 / total_tasks;

restart:

    sprintf (name, "random_task_set_%d", num);

    ts = create_task_set (total_tasks, 25, 25, total_tasks,
                          name,
                          1000, 0, 0, 0,
                          analysis);

    for (i=0; i<total_tasks; i++) {
        do {
            int deadline_unique;
            do {
                D = (rand_long() % max_deadline) + multiplier;
                deadline_unique = TRUE;
                for (j=0; j<i; j++) {
                    if (D == dls[j]) deadline_unique = FALSE;
                }
            }
            while (!deadline_unique);
            dls[i] = D;
            if (has_independent_deadline) {
                T = (rand_long() % max_deadline) + multiplier;
            }
            else {
                T = D;
            }
            U = Umin + (rand_double() * (Umax - Umin));
            C = (time_value) (U * T);
        }
        while (C < multiplier);

        J = 0;
        if (has_jitter == 2) {
            if (rand_double()<0.5) {
                J = (rand_long() % T) / 2;
            }
        }

        sprintf (name, "t%d", i);
        new_task (ts,
                  (C/multiplier)*multiplier,
                  (T/multiplier)*multiplier,
                  (T/multiplier)*multiplier, 1,
                  (D/multiplier)*multiplier,
                  (J/multiplier)*multiplier, 0, name);
    }

    if (utilization_set (ts) >= 1.0) {
        free_task_set (ts);
        goto restart;
    }

    if (has_jitter == 1) {
        int t = rand_long()%total_tasks;
        set_jitter (ts, t, rand_long()%(get_period(ts, t)/2));
    }

    return ts;
}

struct task_set* create_random_task_set_with_utilization (int num,
        int total_tasks,
        int max_deadline,
        const char* analysis,
        int multiplier,
        double set_u,
        int has_jitter,
        int has_independent_deadline)
{
    struct task_set* ts;
    time_value C, T, D, J;
    double U, Umin, Umax, Usum;
    int i, j;
    char name[30];
    time_value dls[1000];

    Umin = 1.0 / max_deadline;
    if(set_u==0){
    	Umax = 2.0 / total_tasks;
    }
    else{
    	Umax = set_u*2 / total_tasks;
    }

restart:
    Usum = 0;
    sprintf (name, "random_task_set_%d", num);

    ts = create_task_set (total_tasks, 25, 25, total_tasks,
                          name,
                          10000, 0, 0, 0,
                          analysis);

    for (i=0; i<total_tasks; i++) {
        do {
            int deadline_unique;
            do {
                D = (rand_long() % max_deadline) + multiplier;
                deadline_unique = TRUE;
                for (j=0; j<i; j++) {
                    if (D == dls[j]) deadline_unique = FALSE;
                }
            }
            while (!deadline_unique);
            dls[i] = D;
            if (has_independent_deadline) {
                T = (rand_long() % max_deadline) + multiplier;
            }
            else {
                T = D;
            }
#if 0
            if(set_u==0){
            	U = Umin + (rand_double() * (Umax - Umin));
            }
            else{
				if(i < total_tasks-1) {
					U = Umin + (rand_double() * (Umax - Umin));
				}
				else {
					if(Usum >= set_u) {
						free_task_set (ts);
						goto restart;
					}
					else {
						U = set_u - Usum;
					}
				}
            }
#else
            U = Umin + (rand_double() * (Umax - Umin));
#endif
            C = (time_value) (U * T);
            if(0==C){
            	goto restart;
            }
        }
        while (C < multiplier);
        Usum += U;

        J = 0;
        if (has_jitter == 2) {
            if (rand_double()<0.5) {
                J = (rand_long() % T) / 2;
            }
        }

        sprintf (name, "t%d", i);
#ifdef USE_DVS
        new_dvs_task (ts,
                      (C/multiplier)*multiplier,
                      (T/multiplier)*multiplier,
                      (T/multiplier)*multiplier, 1,
                      (D/multiplier)*multiplier,
                      (J/multiplier)*multiplier, 0,
                      MAX_FREQ_LEVEL,
                      name);
    }
#else
        new_task (ts,
                  (C/multiplier)*multiplier,
                  (T/multiplier)*multiplier,
                  (T/multiplier)*multiplier, 1,
                  (D/multiplier)*multiplier,
                  (J/multiplier)*multiplier, 0, name);
    }
#endif
#if 0
    if (abs(utilization_set (ts)-set_u) >= UTILIZATION_ACCURACY)
    {
        free_task_set (ts);
        goto restart;
    }
#endif

    if (has_jitter == 1)
    {
        int t = rand_long()%total_tasks;
        set_jitter (ts, t, rand_long()%(get_period(ts, t)/2));
    }

    return ts;
}
