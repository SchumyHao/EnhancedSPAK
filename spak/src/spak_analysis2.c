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

#define DBG_LEVEL 0

/*
 * code in this file implements the full schedulability test from of
 * "An Extendible Approach to..." by Tindell 92 
 */

static int analysis2_valid (struct task_set *ts)
{
  if (max_resp == -1) return FALSE;
  if (!is_all_preemptible (ts)) return FALSE;
  if (has_constraints (ts)) return FALSE;
  return TRUE;
}

static time_value calcFj (struct task_set *ts,
			  time_value lastwiq,
			  int j)
{
  return ((ts->tasks[j].J + lastwiq) / ts->tasks[j].T);
}

static time_value my_sum (struct task_set *ts,
			  int i,
			  time_value lastwiq)
{
  time_value sum, Fj, add;
  int j;

  sum = 0;
  for (j=0; j<ts->num_tasks; j++) {
    if (ts->tasks[j].P <= ts->tasks[i].P && j != i) {
      Fj = calcFj (ts, lastwiq, j);
      add = 
	(tvmin (ts->tasks[j].n,
		div_ceil (ts->tasks[j].J + lastwiq - (Fj * ts->tasks[j].T), 
			  ts->tasks[j].t)) + 
	 (Fj * ts->tasks[j].n)) *
	ts->tasks[j].C;
      sum += add;
    }
  }
  
  return sum;
}

static time_value tickoverheads (struct task_set *ts,
				 time_value lastwiq)
{
  time_value K;
  time_value L;
  time_value Fj;
  int j;
  
  K = 0;
  for (j=0; j<ts->num_tasks; j++) {
    Fj = calcFj (ts, lastwiq, j);
    K += 
      tvmin (div_ceil (ts->tasks[j].J + 
		       lastwiq - 
		       (ts->tasks[j].T * Fj),
		       ts->tasks[j].t),
	     ts->tasks[j].n) +
      (ts->tasks[j].n * Fj);
  }

  L = div_ceil (lastwiq, ts->Tclk);  

  return 
    (L * ts->Cclk) + 
    (tvmin (L, K) * ts->Cql) + 
    (tvmax (K - L, 0) * ts->Cqs);
}
  
static time_value calc_wi (struct task_set *ts,
			   int i,
			   time_value Mi,
			   time_value ni,
			   time_value mi,
			   time_value Ci,
			   time_value Bi)
{
  time_value wiq, lastwiq;

  wiq = 0;
  do {
    time_value ter1, ter2, ter3, ter4;

    lastwiq = wiq;
    
    ter1 = (((Mi * ni) + mi + 1) * Ci);
    ter2 = Bi;
    ter3 = my_sum (ts, i, lastwiq);
    ter4 = tickoverheads (ts, lastwiq);

    wiq = ter1 + ter2 + ter3 + ter4;
    
    DBGPrint (3, ("      wiq = %d (%d + %d + %d + %d)\n",
		  wiq, ter1, ter2, ter3, ter4));
    
  } while (wiq != lastwiq && wiq > 0 && wiq < max_resp);

  if (wiq == lastwiq) {
    return wiq;
  } else {
    return max_resp;
  }
}

static time_value analysis2_response_time (struct task_set *ts, 
					   int i,
					   time_value initial_guess)
{
  time_value ri, wiq, Ji, Mi, mi, ni, ti, Ti, Ci, Bi;
  int q;

  assert (ts);

  Ji = ts->tasks[i].J;
  ni = ts->tasks[i].n;
  ti = ts->tasks[i].t;
  Ti = ts->tasks[i].T;
  Ci = ts->tasks[i].C;
  Bi = ts->tasks[i].B;

  DBGPrint (3, ("  Analysis2: calculating response time for task %s\n", 
		ts->tasks[i].name));

  ri = 0;
  q = 0;
  Mi = q / ni;
  mi = q - (Mi * ni);
  do {

    DBGPrint (3, ("    q = %d\n", q));

    wiq = calc_wi (ts, i, Mi, ni, mi, Ci, Bi);
    ri = tvmax (
		ri, 
		wiq + Ji - (mi * ti) - (Mi * Ti)
		);

    q++;
    Mi = q / ni;
    mi = q - (Mi * ni);
  } while (wiq > ((Mi * Ti) + (mi * ti) - Ji) &&
	   ri < max_resp);

  if (ri < max_resp) {
    return ri;
  } else {
    return max_resp;
  }
}

int get_analysis2_ptrs (const char *id,
			struct spak_analysis *A)
{
  if (strcmp (id, "Tindell92_general") == 0) {
    A->valid = analysis2_valid;
    A->response_time = analysis2_response_time;
    return TRUE;
  } else {
    return FALSE;
  }
}

