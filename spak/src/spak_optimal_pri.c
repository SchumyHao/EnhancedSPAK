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
 * optimal priority assignment from Audsley 91
 */
int assign_optimal_pri (struct task_set *ts)
{
  int j, ordered, preemptible, good;
  
  // print_task_set (ts);

  assert (ts);
  assert (ts->Analysis.valid (ts));

  /*
   * don't use this for mixed-mode task sets; they have no optimal
   * algorithm with quadratic run time
   */
  assert (is_all_preemptible (ts) || is_all_nonpreemptible (ts));

  if (is_all_preemptible (ts)) {
    preemptible = TRUE;
  } else {
    preemptible = FALSE;
  }

  DBGPrint (5, ("assign_optimal_pri (%s):\n",
		(preemptible)?"preemptible":"non-preemptible"));

  // initial ordering is arbitrary
  for (j=0; j<ts->num_tasks; j++) {
    ts->tasks[j].P = j;
  }

  ordered = ts->num_tasks - 1;
  
  do {

    good = FALSE;

    for (j=0; j<ts->num_tasks; j++) {
      time_value rj;

      int Pj = ts->tasks[j].P;

      if (Pj > ordered) continue;

      if (!barriers_permit_pri (ts, j, ordered)) continue;

      assign_pri (ts, j, ordered);

      /*
       * this is an inefficient way to keep preemption thresholds
       * synchronized with priorities...
       */
      if (preemptible) {
    	make_all_preemptible (ts);
      } else {
	    make_all_nonpreemptible (ts);
      }

      calculate_blocking_pcp (ts);

      rj = ts->Analysis.response_time (ts, j, ts->tasks[j].C); 
      DBGPrint (5, ("  task %d: r = %d, D = %d\n",
		    j, rj, ts->tasks[j].D));
      if (rj <= ts->tasks[j].D) {
		// schedulable
		DBGPrint (5, ("  task %d is schedulable at pri %d\n",
				  j, ordered));
		ordered--;
		good = TRUE;
      } else {
		DBGPrint (5, ("  task %d not schedulable at pri %d\n",
				  j, ordered));
		// not schedulable
		assign_pri (ts, j, Pj);
      }
    }

  } while (ordered >= 0 && good);

  DBGPrint (5, ("  assign_optimal_pri returning %s\n",
		(good)?"success":"failure"));

  if (good) assert (feasible (ts, TRUE) == ts->num_tasks);

  return good;
}
