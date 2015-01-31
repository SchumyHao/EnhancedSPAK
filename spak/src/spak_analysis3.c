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

static int analysis3_valid (struct task_set *ts)
{
  if (max_resp == -1) {
    DBGPrint (1, ("max_resp not set\n"));
    return FALSE;
  }

  if (!is_all_preemptible (ts)) {
    DBGPrint (1, ("not everyone is preemptible\n"));
    return FALSE;
  }

  if (is_ts_sporadically_periodic (ts)) {
    DBGPrint (1, ("somebody is sporadically periodic\n"));
    return FALSE;
  }

  /*
  if (has_task_clusters (ts)) {
    DBGPrint (1, ("somebody has task clusters\n"));
    return FALSE;
  }
  */

  if (has_overheads (ts)) {
    DBGPrint (1, ("somebody has overheads\n"));
    return FALSE;
  }

  return TRUE;
}

/*
 * code in this file implements the schedulability test from Section 4
 * of "An Extendible Approach to..." by Tindell 92 
 */

static time_value calc_wi (struct task_set *ts,
			   int i,
			   time_value q,
			   time_value Ci,
			   time_value Bi)
{
  time_value wiq, lastwiq;

  wiq = 0;
  do {
    time_value term1, term2;
    int j;

    lastwiq = wiq;    
    term1 = ((q + 1) * Ci) + Bi;
    term2 = 0;
    
    for (j=0; j<ts->num_tasks; j++) {
      if (ts->tasks[j].P <= ts->tasks[i].P && j != i) {      
	term2 += 
	  div_ceil (ts->tasks[j].J + lastwiq,
		    ts->tasks[j].T) * 
	  ts->tasks[j].C;
      }
    }

    wiq = term1 + term2;    
    DBGPrint (3, ("      wi(%d) = %d + %d = %d\n", q, term1, term2, wiq));
    
  } while (wiq != lastwiq && wiq > 0 && wiq < max_resp);

  if (wiq == lastwiq) {
    return wiq;
  } else {
    return max_resp;
  }
}

static time_value analysis3_response_time (struct task_set *ts, 
					   int i,
					   time_value initial_guess)
{
  time_value ri, wiq, Ji, Ti, Ci, Bi;
  int q;

  assert (ts);

  Ji = ts->tasks[i].J;
  Ti = ts->tasks[i].T;
  Ci = ts->tasks[i].C;
  Bi = ts->tasks[i].B;

  DBGPrint (3, ("  Analysis3: calculating response time for task %s\n", ts->tasks[i].name));

  ri = 0;
  q = 0;
  do {
    time_value thisri;

    wiq = calc_wi (ts, i, q, Ci, Bi);
    DBGPrint (3, ("    wi(%d) = %d\n", q, wiq));
    thisri = wiq + Ji - (q * Ti);
    DBGPrint (3, ("    ri = %d - (%d * %d) = %d\n",
		  wiq, Ji, (q*Ti), thisri));
    ri = tvmax (ri, thisri);
    q++;
  } while (wiq > ((q * Ti) - Ji) && ri < max_resp);

  if (ri < max_resp) {
    return ri;
  } else {
    return max_resp;
  }
}

int get_analysis3_ptrs (const char *id,
			struct spak_analysis *A)
{
  if (strcmp (id, "Tindell92_restricted") == 0) {
    A->valid = analysis3_valid;
    A->response_time = analysis3_response_time;
    return TRUE;
  } else {
    return FALSE;
  }
}

