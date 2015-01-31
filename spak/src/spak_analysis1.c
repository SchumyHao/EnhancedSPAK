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
 * code in this file implements the recurrence relation in Equation 5
 * of "Static Priority Pre-emptive Scheduling", Audsley et al. 92
 */

#include "spak_public.h"
#include "spak_internal.h"

#define DBG_LEVEL 0

static int analysis1_valid (struct task_set *ts)
{
  int i;

  if (max_resp == -1) return FALSE;

  for (i=0; i<ts->num_tasks; i++) {
    if (ts->tasks[i].D > ts->tasks[i].T) {
      return FALSE;
    }
  }

  if (is_ts_sporadically_periodic (ts)) return FALSE;

  if (!is_all_preemptible (ts)) return FALSE;

  if (has_constraints (ts)) return FALSE;

  if (has_overheads (ts)) return FALSE;

  return TRUE;
}   

static time_value analysis1_response_time (struct task_set *ts, 
					   int i,
					   time_value initial_guess)
{
  int j, resp, prevresp;
  struct task *ti;

  DBGPrint (3, ("  Analysis1: calculating response time for task %s\n", 
		ts->tasks[i].name));

  ti = &ts->tasks[i];
  resp = initial_guess;
  do {
    prevresp = resp;
    resp = ti->C + ti->B;
    DBGPrint (3, ("    starting with C = %d,  B = %d\n", ti->C, ti->B));
    for (j=0; j<ts->num_tasks; j++) {
      struct task *tj = &ts->tasks[j];
      if (tj->P <= ti->P && j != i) {
	int inc = div_ceil (prevresp + tj->J, tj->T) * tj->C;
	resp += inc;
	DBGPrint (3, ("      ceil ((%d + %d) / %d) = %d * %d = %d (running total %d)\n", 
		      prevresp,
		      tj->J,
		      tj->T,
		      div_ceil (prevresp + tj->J, tj->T),
		      tj->C,
		      inc,
		      resp));
      }    
    }  
    DBGPrint (3, ("    iteration = %d\n", resp));
    if (resp < 0 || resp > max_resp) {
      // printf ("oops: resp %d exceeds max_resp of %d\n", resp, max_resp);
      return max_resp;
    }
  } while (resp != prevresp);

  return resp + ts->tasks[i].J;
}

int get_analysis1_ptrs (const char *id,
			struct spak_analysis *A)
{
  if (strcmp (id, "Audsley92") == 0) {
    A->valid = analysis1_valid;
    A->response_time = analysis1_response_time;
    return TRUE;
  } else {
    return FALSE;
  }
}

