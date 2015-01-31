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

/*
 * code in this file implements the schedulability test from equations
 * 13 and 14 of "Preemptive and Non-Preemptive Real-Time Uniprocessor
 * Scheduline by Laurent George, Nicolas Rivierre, and Marco Spuri 
 */

#include "spak_public.h"
#include "spak_internal.h"

#define DBG_LEVEL 1

static int analysis4_valid (struct task_set *ts)
{
  if (max_resp == -1) {
    DBGPrint (1, ("max_resp not set\n"));
    return FALSE;
  }

  if (!is_all_nonpreemptible (ts)) {
    DBGPrint (1, ("not everybody is nonpreemptible\n"));
    return FALSE;
  }

  if (is_ts_sporadically_periodic (ts)) {
    DBGPrint (1, ("somebody is sporadically periodic\n"));
    return FALSE;
  }

  if (has_overheads (ts)) {
    DBGPrint (1, ("somebody has overheads\n"));
    return FALSE;
  }

  if (!constraints_valid (ts)) {
    DBGPrint (1, ("constraints invalid\n"));
    print_task_set (ts);
    return FALSE;
  }

  return TRUE;
}

/*
 * find the worst-case blocking due to a task of lower priority
 */
static time_value find_max_lp_C (struct task_set *ts, int i)
{
  int j, max_lp_C = 0;

  for (j=0; j<ts->num_tasks; j++) {
    if (ts->tasks[j].P > ts->tasks[i].P) {
      if ((ts->tasks[j].C-1) > max_lp_C){
	max_lp_C = ts->tasks[j].C-1;
      }
    }
  }
  return max_lp_C;
}

static int findL (struct task_set *ts, int i)
{
  time_value Li, oldLi, j, max_lp_C;

  max_lp_C = find_max_lp_C (ts, i);
  DBGPrint (3, ("    max_lp_C = %d\n", max_lp_C));

  /*
   * unlike all other fixpoint calculations in this sort of analysis,
   * this one can't start at zero
   */
  Li = 1; 

  do {
    oldLi = Li;

    Li = max_lp_C;
    
    DBGPrint (5, ("    Li = %d", Li));

    for (j=0; j<ts->num_tasks; j++) {
      if (ts->tasks[j].P <= ts->tasks[i].P) {
	int term;
	term = div_ceil (oldLi, ts->tasks[j].T) * ts->tasks[j].C;
	DBGPrint (5, (" + %d", term));
	Li += term;
      }
    }
    
    DBGPrint (5, (" = %d\n", Li));

  } while (Li != oldLi && Li < max_resp);

  if (Li == max_resp) {
    return max_resp;
  } else {
    return Li;
  }
}

static int findQ (struct task_set *ts, int i)
{
  int Li;

  Li = findL (ts, i);

  DBGPrint (3, ("    Li = %d\n", Li));

  if (Li == max_resp) {
    return max_resp;
  } else {
    return div_floor (Li, ts->tasks[i].T);
  }
}

static time_value calc_wi (struct task_set *ts,
			   int i,
			   time_value q,
			   time_value Ci)
{
  time_value wiq, lastwiq, max_lp_C;

  max_lp_C = find_max_lp_C (ts, i);

  wiq = 0;
  do {
    time_value term1, term2;
    int j;

    lastwiq = wiq;

    term1 = (q * Ci) + max_lp_C;
    DBGPrint (3, ("      wi(%d) = %d\n", q, term1));

    term2 = 0;    
    for (j=0; j<ts->num_tasks; j++) {
      if (ts->tasks[j].P <= ts->tasks[i].P && j != i) {
	time_value add = (1 + (div_floor (lastwiq + ts->tasks[j].J, ts->tasks[j].T))) * 
	  ts->tasks[j].C;
	term2 += add;
	DBGPrint (3, ("        + ( %d = (1 + %d / %d) * %d )\n", 
		      add, lastwiq, ts->tasks[j].T, ts->tasks[j].C));
      }
    }

    wiq = term1 + term2;    
    DBGPrint (3, ("      wi(%d) = %d (%d + %d)\n", q, wiq, term1, term2));
    
  } while (wiq != lastwiq && wiq > 0 && wiq < max_resp);

  if (wiq == lastwiq) {
    return wiq;
  } else {
    return -1;
  }
}

static time_value analysis4_response_time (struct task_set *ts, 
					    int i,
					    time_value initial_guess)
{
  time_value max_ri, wiq, Ti, Ci, Ji;
  int q, Q;

  assert (ts);

  Ti = ts->tasks[i].T;
  Ci = ts->tasks[i].C;
  Ji = ts->tasks[i].J;

  DBGPrint (3, ("  Analysis4: calculating response time for task %s\n", 
		ts->tasks[i].name));

  max_ri = 0;
  Q = findQ (ts, i);

  DBGPrint (3, ("    Q = %d\n\n", Q));

  if (Q == max_resp) return max_resp;

  for (q=0; q<=Q; q++) {
    time_value ri;

    DBGPrint (3, ("    q = %d\n", q));

    wiq = calc_wi (ts, i, q, Ci);
    if (wiq == -1) return max_resp;

    DBGPrint (3, ("    wi(%d) = %d\n", q, wiq));
    ri = wiq + Ci + Ji - (q * Ti);
    DBGPrint (3, ("    ri = %d + %d - (%d * %d) = %d\n",
		  wiq, Ci, q, Ti, ri));
    DBGPrint (3, ("\n"));

    max_ri = tvmax (max_ri, ri);
  }

  if (max_ri < max_resp) {
    return max_ri;
  } else {
    return max_resp;
  }
}

int get_analysis4_ptrs (const char *id,
			struct spak_analysis *A)
{
  if (strcmp (id, "George96") == 0) {
    A->valid = analysis4_valid;
    A->response_time = analysis4_response_time;
    return TRUE;
  } else {
    return FALSE;
  }
}
