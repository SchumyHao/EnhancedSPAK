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

#define DBG_LEVEL 1

/*
 * perform analysis, return number of feasible tasks
 */
int feasible (struct task_set* ts, int all)
{
    int i;
    int feas = 0;
    double U;

    assert (ts);
    assert (ts->Analysis.valid (ts));

    // initially, nobody is schedulable
    for (i=0; i<ts->num_tasks; i++) {
        ts->tasks[i].S = 0;
    }

    U = utilization_set (ts);

    DBGPrint (3, ("U = %f\n", U));

    if (U > 1.0) {
        DBGPrint (3, ("  feasible is bailing since utilization is > 1.0\n"));
        return 0;
    }

    for (i=0; i<ts->num_tasks; i++) {
        ts->tasks[i].R = ts->Analysis.response_time (ts, i, 0);
        DBGPrint (3, ("  task %d: r=%d, D=%d\n",
                      i, ts->tasks[i].R, ts->tasks[i].D));
        if (ts->tasks[i].R <= ts->tasks[i].D) {
            feas++;
            ts->tasks[i].S = 1;
        }
        else {
            if (!all) goto out;
        }
    }

out:
    return feas;
}

int feasible_one_task (struct task_set* ts, int i)
{
    double U;

    assert (ts);
    assert (ts->Analysis.valid (ts));

    // initially, no schedulable
    ts->tasks[i].S = 0;

    U = utilization_set (ts);

    DBGPrint (3, ("U = %f\n", U));

    if (U > 1.0) {
        DBGPrint (3, ("  feasible is bailing since utilization is > 1.0\n"));
        return 0;
    }

    ts->tasks[i].R = ts->Analysis.response_time (ts, i, 0);
    DBGPrint (3, ("  task %d: r=%d, D=%d\n",
                  i, ts->tasks[i].R, ts->tasks[i].D));
    if (ts->tasks[i].R <= ts->tasks[i].D) {
        ts->tasks[i].S = 1;
        return 1;
    }
    else {
        return 0;
    }
}

