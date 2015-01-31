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
 * code in this file implements a corrected version of the analysis
 * from Wang and Saksena's preemption threshold papers in RTCSA and
 * RTSS 
 */

#include "spak_public.h"
#include "spak_internal.h"

#define DBG_LEVEL 2
#define FINDL_LEVEL 3

static int analysis6_valid (struct task_set *ts)
{
  if (max_resp == -1) {
    DBGPrint (1, ("max_resp not set!\n"));
    return FALSE;
  }

  if (is_ts_sporadically_periodic (ts)) {
    DBGPrint (1, ("task set is sporadically periodic!\n"));
    return FALSE;
  }

  if (has_overheads (ts)) {
    DBGPrint (1, ("task set has overheads!\n"));
    return FALSE;
  }

  if (!is_pri_unique (ts)) {
    DBGPrint (1, ("priorities are not unique!\n"));
    return FALSE;
  }

  if (is_thresh_lower_than_pri (ts)) {
    DBGPrint (1, ("there exists a preemption threshold lower than a priority!\n"));
    print_task_set (ts);
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
 * worst-case blocking due to tasks with lower priority
 * but higher preemption threshold
 */
static time_value find_max_block (struct task_set *ts, int i)
{
  int j;
  time_value maxb = 0;

  for (j=0; j<ts->num_tasks; j++) {
    if (ts->tasks[j].PT <= ts->tasks[i].P &&
	(ts->tasks[i].P <= ts->tasks[j].P && i!=j)) {
#if 0
      if (ts->tasks[j].C > maxb) {
	maxb = ts->tasks[j].C;
      }
#else
      if (ts->tasks[j].C-1 > maxb) {
	maxb = ts->tasks[j].C-1;
      }
#endif
    }
  }
  
  return maxb;
}

/*
 * find length of a level-i busy period (where i is the priority of
 * the task passed in as an argument)
 */
static int findL (struct task_set *ts, int i)
{
  time_value Li, oldLi, j, max_block;

  max_block = find_max_block (ts, i);
  DBGPrint (FINDL_LEVEL, ("    max_block = %d\n", max_block));

  /*
   * unlike all other fixpoint calculations in this sort of analysis,
   * this one can't start at zero
   */
  Li = 1; 

  do {
    oldLi = Li;

    Li = max_block;
    
    DBGPrint (FINDL_LEVEL, ("    L(%d) = %d", ts->tasks[i].P, Li));

    for (j=0; j<ts->num_tasks; j++) {
      if (ts->tasks[j].P <= ts->tasks[i].P) {
	int term;

	// FIXME: not sure which line is right
	term = div_ceil (oldLi + ts->tasks[j].J, ts->tasks[j].T) * ts->tasks[j].C;
	// term = div_ceil (oldLi, ts->tasks[j].T) * ts->tasks[j].C;

	DBGPrint (FINDL_LEVEL, (" + (div_ceil(%d + %d, %d)*%d\n", 
				oldLi, ts->tasks[j].J,
				ts->tasks[j].T, ts->tasks[j].C));
	// DBGPrint (FINDL_LEVEL, (" + %d", term));
	Li += term;
      }
    }
    
    DBGPrint (FINDL_LEVEL, (" = %d\n", Li));

  } while (Li != oldLi && Li < max_resp);

  if (Li >= max_resp) {
    return max_resp;
  } else {
    DBGPrint (FINDL_LEVEL, ("L(%d) = %d\n", ts->tasks[i].P, Li));
    return Li;
  }
}

static int findQ (struct task_set *ts, int i)
{
  int Li;

  Li = findL (ts, i);

  DBGPrint (3, ("    L(%d) = %d\n", ts->tasks[i].P, Li));

  if (Li == max_resp) {
    return max_resp;
  } else {
    return div_floor (Li, ts->tasks[i].T);
  }
}

static time_value find_start_time (struct task_set *ts, int i, int q)
{
  time_value siq, prev_siq;
  int j;

  siq = 0;
  do {
    prev_siq = siq;
    siq = find_max_block (ts, i) + (q * ts->tasks[i].C);
    DBGPrint (5, ("        siq(%d) = %d + %d = %d\n", 
		  q, 
		  find_max_block (ts, i),
		  (q * ts->tasks[i].C),
		  siq));

    for (j=0; j<ts->num_tasks; j++) {
      if (ts->tasks[j].P <= ts->tasks[i].P && i != j) {
	time_value add = 
	  (1 + div_floor (prev_siq + ts->tasks[j].J, ts->tasks[j].T)) * ts->tasks[j].C;
	siq += add;
	DBGPrint (5, ("          + %d\n", add));
      }
    }
    DBGPrint (5, ("          = %d\n", siq));
  } while (siq != prev_siq && siq < max_resp);

  if (siq < max_resp) {
    return siq;
  } else {
    return max_resp;
  }
}

static time_value find_finish_time (struct task_set *ts, int i, int q)
{
  int j, rep;
  time_value start_time, fiq, prev_fiq;

  start_time = find_start_time (ts, i, q);
  if (start_time == max_resp) {
    return max_resp;
  }

  fiq = 0;
  rep = 0;

  do {
    if (rep++ > 100000) {
      print_task_set (ts);
      assert (0);
    }

    prev_fiq = fiq;

    fiq = start_time + ts->tasks[i].C;
    DBGPrint (5, ("        fiq(%d) = %d + %d = %d\n", 
		  q,
		  start_time,
		  ts->tasks[i].C,
		  fiq));

    for (j=0; j<ts->num_tasks; j++) {
      if (ts->tasks[j].P < ts->tasks[i].PT) {
	time_value add = 
	  (div_ceil (prev_fiq + ts->tasks[j].J, ts->tasks[j].T) - 
	   (1 + div_floor (start_time + ts->tasks[j].J, ts->tasks[j].T)))
	  * ts->tasks[j].C;
	fiq += add;
	DBGPrint (5, ("          + %d    (%d - %d)*%d\n", 
		      add,
		      div_ceil (prev_fiq + ts->tasks[j].J, ts->tasks[j].T),
		      1 + div_floor (start_time + ts->tasks[j].J, ts->tasks[j].T),
		      ts->tasks[j].C));
      }
    }
    DBGPrint (5, ("          = %d\n", fiq));
  } while (fiq != prev_fiq && fiq < max_resp);

  if (fiq < max_resp) {
    return fiq;
  } else {
    return max_resp;
  }
}

static time_value analysis6_response_time (struct task_set *ts,
					   int i,
					   time_value initial_guess)
{
  int q, Q;
  time_value finish_time, ri, max_ri;

  DBGPrint (3, ("\n  Analysis6: calculating response time for task %s\n", 
		ts->tasks[i].name));

  max_ri = 0;
  ri = 0;

  Q = findQ (ts, i);

  DBGPrint (3, ("    Q = %d\n\n", Q));

  // Q += 3;

  if (Q == max_resp) return max_resp;

  for (q=0; q<=Q; q++) {
  
    DBGPrint (3, ("    q = %d\n", q));

    finish_time = find_finish_time (ts, i, q);
    ri = finish_time + ts->tasks[i].J - (q*ts->tasks[i].T);

    DBGPrint (4, ("      ri(%d) = %d - %d*%d = %d\n",
		  q, finish_time, q, ts->tasks[i].T, ri));

    if (ri > max_ri) {
      max_ri = ri;
    }
  } 

  DBGPrint (3, ("    max ri was %d\n", max_ri));

  if (max_ri < max_resp) {
    return max_ri;
  } else {
    return max_resp;
  }
}

int get_analysis6_ptrs (const char *id,
			struct spak_analysis *A)
{
  if (strcmp (id, "Wang00_fixed") == 0) {
    A->valid = analysis6_valid;
    A->response_time = analysis6_response_time;
    return TRUE;
  } else {
    return FALSE;
  }
}
