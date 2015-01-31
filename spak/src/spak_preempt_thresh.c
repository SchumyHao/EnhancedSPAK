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
#include "math.h"

#define DBG_LEVEL 0

/*
 * analysis can fail to converge if this is true
 */
int is_thresh_lower_than_pri (struct task_set *ts)
{
  int i;

  for (i=0; i<ts->num_tasks; i++) {
    if (ts->tasks[i].P < ts->tasks[i].PT) return TRUE;
  }

  return FALSE;
}

/*
 * ensure that a task set uses the analysis for preemption thresholds
 */
int uses_preempt_thresh_analysis (struct task_set *ts)
{
  struct spak_analysis A;
  if ((internal_set_analysis (&A, "Wang00") && 
       A.response_time == ts->Analysis.response_time) ||
      (internal_set_analysis (&A, "Wang00_fixed") && 
       A.response_time == ts->Analysis.response_time)
#ifdef USE_DVS
       || (internal_set_analysis (&A, "ee_fppt") &&
       A.response_time == ts->Analysis.response_time)
#endif
  ) {
    return TRUE;
  } else {
    return FALSE;
  }
}

static int np_constr_wrong (struct task_set *ts, int t)
{
  int i, j, found;

  for (i=0; i<ts->num_task_clusters; i++) {
    struct task_cluster *cluster = &ts->task_clusters[i];
    found = FALSE;
    for (j=0; j<cluster->num_tasks; j++) {
      int tj = cluster->tasks[j];
      if (tj == t) found = TRUE;
    }
    if (!found) continue;
    for (j=0; j<cluster->num_tasks; j++) {
      int tj = cluster->tasks[j];
      if (tj == t) continue;
      if (ts->tasks[t].PT > ts->tasks[tj].P) {
	return TRUE;
      }
    }
  }
  
  return FALSE;
}

/*
 * this is the "assign preemption thresholds" algorithm from Figure 2
 * of Wang and Saksena 99
 */
int assign_optimal_preemption_thresholds (struct task_set *ts)
{
  int i, current, found;
  time_value ri;

  assert (uses_preempt_thresh_analysis (ts));
  make_all_preemptible (ts);

  for (current = ts->num_tasks-1; current >= 0; current--) {
    found = FALSE;
    for (i=0; i<ts->num_tasks; i++) {
      if (ts->tasks[i].P == current) {
	assert (!found);
	found = TRUE;
	if (np_constr_wrong (ts, i)) {
	  ts->tasks[i].PT--;
	  assert (ts->tasks[i].PT >= 0);
	}
	respect_constraints (ts);
	ri = ts->Analysis.response_time (ts, i, 0);
	while (ri > ts->tasks[i].D) {
	  ts->tasks[i].PT--;
	  if (ts->tasks[i].PT < 0) {
	    ts->tasks[i].PT = 0;
	    return FALSE;
	  }
	  ri = ts->Analysis.response_time (ts, i, 0);
	}
	DBGPrint (3, ("task %d: ri = %d, dl = %d\n",
		      i, ri, ts->tasks[i].D));
      }
    }
    assert (found);
  }

  return TRUE;
}

/*
 * return how much longer a task's wcrt is than its deadline
 */
static time_value lateness (struct task_set *ts, int i)
{
  ts->tasks[i].R = ts->Analysis.response_time (ts, i, 0);
  return tvmax (0, ts->tasks[i].R - ts->tasks[i].D);
}

static void conditional_swap_task_barriers (struct task_set *ts, 
					  int x, int y)
{
  if (ts->task_barriers[x] < ts->task_barriers[y]) {
    int tmp = ts->task_barriers[x];
    ts->task_barriers[x] = ts->task_barriers[y];
    ts->task_barriers[y] = tmp;
  }
}

/*
 * sort preemption threshold barriers for a task set from lowest to
 * highest priority (in descending order, numerically) 
 */
void sort_task_barriers (struct task_set *ts)
{
  int i, j, lowest;

  for (i=0; i<ts->num_task_barriers-1; i++) {
    lowest = -1;
    for (j=i+1; j<ts->num_task_barriers; j++) {
      if (ts->task_barriers[j] > lowest) {
	lowest = j;
      }
    }
    assert (lowest != -1);
    conditional_swap_task_barriers (ts, i, lowest);
  }

  {
    DBGPrint (2, ("sorted task barriers: "));
    for (i=0; i<ts->num_task_barriers; i++) {
      DBGPrint (2, ("%d ", ts->task_barriers[i]));
    }
    DBGPrint (2, ("\n"));
  }
}

/*
 * This is a modified version of the greedy assignment algorithm for
 * assigning priorities and thresholds from Figure 2 of Saksena and
 * Wang 00.  
 */
static int try_schedule_range_pt (struct task_set *ts, 
				  int range_bottom,
				  int range_top, 
				  int pri)
{
  int max, maxlate, i, old_pri;
  
  DBGPrint (5, ("  entering PT search with range %d to %d, pri %d\n", 
		range_bottom, range_top, pri));

  assert (range_bottom >= 0 && range_bottom < ts->num_tasks);
  assert (range_top >= -1 && range_top < ts->num_tasks);
  assert (pri <= range_bottom && pri > range_top);

  max = -1;
  maxlate = -1000000;

  for (i=0; i<ts->num_tasks; i++) {
    if (i > range_bottom || i <= range_top) continue;    
    if (ts->tasks[i].P > pri) continue;
    old_pri = ts->tasks[i].P;
    assign_pri (ts, i, pri);
    if (lateness (ts, i) > maxlate) {
      max = i;
      maxlate = lateness (ts, i);
    }
    // put priority back where it was
    assign_pri (ts, i, old_pri);
  }
  assert (max != -1);

  old_pri = ts->tasks[max].P;
  assign_pri (ts, max, pri);
  if (pri-1 == range_top) {
    return TRUE;
  } else {
    return try_schedule_range_pt (ts, range_bottom, range_top, pri-1);
  }
}

/*
 * max is confusing here: we return the highest priority or
 * threshold, which numerically is lowest
 */
int maxpri (struct task_set *ts, int c)
{
  int max = ts->num_tasks+1;
  int i, p;
  
  for (i=0; i<ts->task_clusters[c].num_tasks; i++) {
    p = ts->tasks[ts->task_clusters[c].tasks[i]].P;
    if (p < max) max = p;
  }

  return max;
}

int minpri (struct task_set *ts, int c)
{
  int min = 0;
  int i, p;
  
  for (i=0; i<ts->task_clusters[c].num_tasks; i++) {
    p = ts->tasks[ts->task_clusters[c].tasks[i]].P;
    if (p > min) min = p;
  }

  return min;
}

static int try_schedule_cluster (struct task_set *ts, 
				 int c, 
				 int pri)
{
  int i, j, k;
  int old_pris[MAX_TASKS_PER_CLUSTER];
  int nextp;
  int max_pri = pri - (ts->task_clusters[c].num_tasks-1);

  DBGPrint (5, ("    try_schedule_cluster: cluster with %d tasks starting at %d\n",
		ts->task_clusters[c].num_tasks, pri));

#define OPTIMAL 1

#ifdef OPTIMAL

  for (i=0; i<ts->task_clusters[c].num_tasks; i++) {
    int t = ts->task_clusters[c].tasks[i];
    ts->tasks[t].PT = max_pri;
    old_pris[i] = ts->tasks[t].P;
    ts->tasks[t].tag = FALSE;
  }
  
  nextp = pri;
  for (i=0; i<ts->task_clusters[c].num_tasks; i++) {

    for (j=0; j<ts->task_clusters[c].num_tasks; j++) {
      int tj = ts->task_clusters[c].tasks[j];
      int oldpri = ts->tasks[tj].P;
      time_value late;

      if (ts->tasks[tj].tag) continue;
      
      assign_only_pri (ts, tj, nextp);
      late = lateness (ts, tj);
      DBGPrint (5, ("      at pri %d, task %d has lateness %d\n",
		    nextp, tj, late));
      if (late == 0) {
	ts->tasks[tj].tag = TRUE;
	goto loop_out;
      }
      assign_only_pri (ts, tj, oldpri);
    }

    for (k=0; k<ts->task_clusters[c].num_tasks; k++) {
      int t = ts->task_clusters[c].tasks[k];
      ts->tasks[t].P = old_pris[k];
    }
    return FALSE;
    
  loop_out:
    nextp--;
  }

  for (i=0; i<ts->task_clusters[c].num_tasks; i++) {
    int ti = ts->task_clusters[c].tasks[i];
    DBGPrint (5, ("      workable: task %d is pri %d, pt %d, R %d, D %d\n",
		  ti, ts->tasks[ti].P, ts->tasks[ti].PT,
		  ts->tasks[ti].R, ts->tasks[ti].D));
  }
  return TRUE;

#else
  
  {
    int schedulable;
    int old_pts[MAX_TASKS_PER_CLUSTER];
    
    // save previous priorities for this cluster
    for (i=0; i<ts->task_clusters[c].num_tasks; i++) {
      int t = ts->task_clusters[c].tasks[i];
      old_pris[i] = ts->tasks[t].P;
      old_pts[i] = ts->tasks[t].P;
    }
    
    // assign new priorities
    nextp = pri;
    for (i=0; i<ts->task_clusters[c].num_tasks; i++) {
      int t = ts->task_clusters[c].tasks[i];
      assign_pri (ts, t, nextp);
      nextp--;
      ts->tasks[t].PT = max_pri;
    }
    
    schedulable = TRUE;
    for (i=0; i<ts->task_clusters[c].num_tasks; i++) {
      int t = ts->task_clusters[c].tasks[i];
      DBGPrint (2, ("    can_schedule_cluster: task %d has pri %d and pt %d",
		    t, ts->tasks[t].P, ts->tasks[t].PT));
      if (ts->Analysis.response_time (ts, t, 0) > ts->tasks[t].D) {
	schedulable = FALSE;
	DBGPrint (2, (" (not schedulable)\n"));
      } else {
	DBGPrint (2, (" (schedulable)\n"));
      }
    }
    if (schedulable) return TRUE;
    
    // not schedulable, so restore previous priorities
    for (i=0; i<ts->task_clusters[c].num_tasks; i++) {
      int t = ts->task_clusters[c].tasks[i];
      assign_pri (ts, t, old_pris[i]);
      ts->tasks[t].PT = pri;
    }
    
  }

  return FALSE;

#endif
}

void print_cluster (struct task_set *ts, int c)
{
  int i;

  DBGPrint (0, ("      cluster %d : \n", c));
  for (i=0; i<ts->task_clusters[c].num_tasks; i++) {
    int t = ts->task_clusters[c].tasks[i];
    DBGPrint (0, ("        task %d, pri %d, pt %d\n",
		  t,
		  ts->tasks[t].P,
		  ts->tasks[t].PT));
  }
}

/*
 * optimal assignment for task clusters without runtime support for PT
 */
static int try_schedule_range_npt (struct task_set *ts, 
				   int range_bottom,
				   int range_top,
				   int pri)
{
  int i;

  DBGPrint (5, ("  entering NPT search with range %d to %d, pri %d\n", 
		range_bottom, range_top, pri));

  assert (range_bottom >= 0 && range_bottom < ts->num_tasks);
  assert (range_top >= -1 && range_top < ts->num_tasks);

  if (pri == range_top) {
    return TRUE;
  }

  for (i=0; i<ts->num_task_clusters; i++) {
    int min_p = minpri (ts, i);
    int max_p = maxpri (ts, i);
    if (min_p > pri) {
      DBGPrint (5, ("    cluster oob below (minp %d, maxp %d, pri %d, range top %d)\n",
		    min_p, max_p, pri, range_top));
      continue;    
    }
    if (ts->task_clusters[i].tasks[0] <= range_top) {
      DBGPrint (5, ("    cluster oob above (minp %d, maxp %d, pri %d, range top %d)\n",
		    min_p, max_p, pri, range_top));
      continue;    
    }
    DBGPrint (5, ("    about to try to schedule cluster %d\n", i));
  
    if (try_schedule_cluster (ts, i, pri)) {
      int val;
      DBGPrint (5, ("    CAN schedule cluster: %d at pri %d\n", i, pri));
      assert (ts->task_clusters[i].num_tasks > 0);
      val = try_schedule_range_npt (ts, 
				    range_bottom, 
				    range_top, 
				    pri - ts->task_clusters[i].num_tasks);
      return val;
    } else {
      DBGPrint (5, ("    CANNOT schedule cluster: %d at pri %d (minp %d, maxp %d)\n", 
		    i, pri, min_p, max_p));
    }
  }
  
  return FALSE;
}

static int try_ranges (struct task_set *ts, 
			       int range_bottom,
			       int barnum,
			       int target_pt_support)
{
  int res;

  DBGPrint (2, ("entering try_ranges, barnum = %d, bar value = %d\n", 
		barnum, 
		(barnum < ts->num_task_barriers) ? ts->task_barriers[barnum] : -1));
  assert (barnum <= ts->num_task_barriers);

  if (barnum >= ts->num_task_barriers) {
    if (target_pt_support) {
      res = try_schedule_range_pt (ts, 
				   range_bottom, 
				   -1, 
				   range_bottom);
      if (!res) return FALSE;
      return assign_optimal_preemption_thresholds (ts);
    } else {
      return try_schedule_range_npt (ts, 
				     range_bottom, 
				     -1,
				     range_bottom);
    }
  } else {
    if (target_pt_support) {
      res =  try_schedule_range_pt (ts, 
				    range_bottom, 
				    ts->task_barriers[barnum],
				    range_bottom);
    } else {
      res = try_schedule_range_npt (ts, 
				    range_bottom, 
				    ts->task_barriers[barnum],
				    range_bottom);
    }
    if (!res) return FALSE;
    return try_ranges (ts, 
		       ts->task_barriers[barnum], 
		       barnum+1,
		       target_pt_support);
  }
}

/*
 * entry point for greedy assignment of priorities and thresholds
 */
int greedy_priorities_and_thresholds (struct task_set *ts,
				      int target_pt_support)
{
  int res;

  assert (ts);

  DBGPrint (2, ("entering greedy_priorities_and_thresholds (%s target PT support)\n",
		(target_pt_support) ? "WITH" : "WITHOUT"));

  sort_task_barriers (ts);

  create_clusters_for_singletons (ts);

  assert (are_all_tasks_in_clusters (ts));
  set_priorities (ts, BY_CLUSTER);
  make_all_preemptible (ts);
  if (has_constraints (ts)) {
    respect_constraints (ts);
  }
  assert (ts->Analysis.valid (ts));
  
  // FIXME
  if (target_pt_support) return FALSE;

  res = try_ranges (ts, ts->num_tasks-1, 0, target_pt_support);

#if 0
  if (target_pt_support) {
    assign_optimal_preemption_thresholds (ts);
    respect_constraints (ts);
    if (feasible (ts, TRUE) == num_tasks (ts)) {
      res = TRUE;
    } else {
      res = FALSE;
    }
  }
#endif

  if (res) {
    assert (ts->Analysis.valid (ts));
    if (feasible (ts, TRUE) != num_tasks (ts)) {
      print_task_set (ts);
      assert (0);
    }
  }
  
  return res;
}

void create_clusters_for_singletons (struct task_set *ts)
{
  int i;

  // create clusters for all singleton tasks
  
  for (i=0; i<ts->num_tasks; i++) {
    if (!in_cluster (ts, i)) {
      struct task_cluster *cluster = new_task_cluster (ts, ts->tasks[i].name);
      add_to_task_cluster (ts, cluster, ts->tasks[i].name);
    }
  }
}

static int total, feas;

/*
 * use recursion to exhaustively try all preemption threshold assignments
 */
static void pick_pt (struct task_set *ts, 
		     int t)
{
  int pt;
  int n = ts->num_tasks;

  for (pt = 0; pt < n; pt++) {

    // bail if threshold is lower (higher, numerically!) than priority
    if (pt > ts->tasks[t].P) goto loop_end;

    // assign priority
    ts->tasks[t].PT = pt;

    if (t == n-1) {
      /*
       * base case: all thresholds assigned, so test feasibility
       */
      total++;
      if (feasible (ts, FALSE) == ts->num_tasks) {
	feas++;
      }
    } else {
      /*
       * still more tasks to go, so recurse
       */
      pick_pt (ts, t+1);
    } 
  loop_end:
    ;
  }
}

/*
 * use recursion to exhaustively try all priority assignments
 */
static void pick_pri (struct task_set *ts, 
		      int t)
{
  int i, pri;
  int n = ts->num_tasks;

  for (pri = 0; pri < n; pri++) {
    
    // bail if collision with a lower-numbered task
    for (i=0; i<t; i++) {
      if (ts->tasks[i].P == pri) goto loop_end;
    }
    
    // no collision, so assign priority
    ts->tasks[t].P = pri;

    if (t == n-1) {
      /*
       * base case: all priorities assigned
       */
      pick_pt (ts, 0);
    } else {
      /*
       * still more tasks to go, so recurse
       */
      pick_pri (ts, t+1);
    } 
  loop_end:
    ;
  }
}

/*
 * test all possible assignments of priorities and preemption thresholds
 *
 * this is O(n!*n!) -- ouch!
 */
int exhaustive_assign_optimal_priorities_and_thresholds (struct task_set *ts)
{
  assert (ts);
  assert (ts->Analysis.valid (ts));
  total = 0;
  feas = 0;
  pick_pri (ts, 0);
  DBGPrint (2, ("exhaustively tested %d priorities / thresholds: %d were feasible\n", 
		total, feas));
  if (feas == 0) {
    return FALSE;
  } else {
    return TRUE;
  }
}

/*
 * For a given assignment of priorities and preemption thresholds,
 * return the minimum number of non-preemptible groups that the task
 * set can be divided into.
 *
 * This is from Figure 3 of Saksena and Wang 00.
 */
int optimal_partition_into_threads (struct task_set *ts)
{
  int nthreads = 0;
  int *pt_map, *scratch;
  int k, j;

  assert (ts);
  assert (uses_preempt_thresh_analysis (ts));
  assert (ts->Analysis.valid (ts));

  pt_map = (int *) xmalloc (sizeof (int) * ts->num_tasks);
  scratch = (int *) xmalloc (sizeof (int) * ts->num_tasks);

  for (k=0; k<ts->num_tasks; k++) {
    scratch[k] = FALSE;
  }

  /*
   * sort tasks by preemption threshold in non-decreasing order 
   */
  for (k=0; k<ts->num_tasks; k++) {
    int max = -1;
    for (j=0; j<ts->num_tasks; j++) {
      if (!scratch[j] && (max == -1 || ts->tasks[j].PT > ts->tasks[max].PT)) {
	max = j;
      }
    }
    assert (max != -1);
    pt_map[k] = max;
    scratch[max] = TRUE;
    DBGPrint (3, ("%d: task %d, p = %d, pt = %d\n",
		  k, max, ts->tasks[max].P, ts->tasks[max].PT));
  }

  xfree (scratch);

  for (k=0; k<ts->num_tasks; k++) {
    int tk = pt_map[k];
    if (tk == -1) continue;
    ts->tasks[tk].thread = nthreads;
    for (j=k+1; j<ts->num_tasks; j++) {
      int tj = pt_map[j];
      if (tj == -1) continue;
      if (ts->tasks[tj].P >= ts->tasks[tk].PT) {
	ts->tasks[tj].thread = nthreads;
	pt_map[j] = -1;
      }
    }
    nthreads++;
  }

  xfree (pt_map);

  DBGPrint (4, ("these %d tasks can be run in %d threads\n",
		num_tasks (ts), nthreads));

  for (k=0; k<nthreads; k++) {
    DBGPrint (4, ("thread %d: ", k));
    for (j=0; j<ts->num_tasks; j++) {
      if (ts->tasks[j].thread == k) {
	DBGPrint (4, ("%d ", j));
      }
    }
    DBGPrint (4, ("\n"));
  }

  for (k=0; k<ts->num_tasks; k++) {
    for (j=0; j<ts->num_tasks; j++) {
      if (j != k && ts->tasks[k].thread == ts->tasks[j].thread) {
	assert (ts->tasks[k].P >= ts->tasks[j].PT &&
		ts->tasks[j].P >= ts->tasks[k].PT);
      } else {
	// FIXME
      }
    }
  }

  return nthreads;
}

int ANNEAL_MAX = -1;
double INIT_TEMP = 0.00;
double TEMP_SCALE = 0.99;

/*
 * Given a feasible assignment of priorities and preemption
 * thresholds, attempt to adjust priorities and thresholds in order to
 * reduce the number of threads onto which they are assigned; this has
 * the obvious side effect of increasing the sensitivity of the task
 * set.
 */
static void anneal_threads_or_cs (struct task_set *orig_ts, 
				  int target,
				  int *optimized,
				  double *best_cs_per_thr,
				  struct task_set **best_ts_per_thr,
				  int target_pt_support)
{
  int best_thr, new_thr;
  double temp = INIT_TEMP;
  struct task_set *new_ts, *best_ts;
  int i;
  int last_improvement = 0;
  double best_cs, new_cs;

  assert (ANNEAL_MAX != -1);

  best_thr = optimal_partition_into_threads (orig_ts);

  assert (target == -1 || target == best_thr);
  assert (feasible (orig_ts, FALSE) == orig_ts->num_tasks);

  best_ts = copy_task_set (orig_ts);
  best_cs = find_critical_scale (orig_ts, NULL);

  free_task_set (orig_ts);
  
  if (!target_pt_support) {
    assert (are_all_tasks_in_clusters (best_ts));
    ensure_target_doesnt_need_pt (best_ts);
  }

  if (best_cs > best_cs_per_thr[best_thr]) {
    best_cs_per_thr[best_thr] = best_cs;
    if (best_ts_per_thr[best_thr]) free_task_set (best_ts_per_thr[best_thr]);
    best_ts_per_thr[best_thr] = copy_task_set (best_ts);
  }

  DBGPrint (1, ("minimize_threads_by_annealing: %d initial threads, target = %d\n", 
		best_thr, target));

  new_cs = 0;
  i = 0;
  while (i < (last_improvement + ANNEAL_MAX)) {

    if (i%100 == 0) DBGPrint (2, ("anneal_threads_or_cs (%d) %d\n", target, i));

    new_ts = copy_task_set (best_ts);

    if (i != 0) {
      if (target_pt_support) {
	permute_pri_and_thresh (new_ts);
	respect_constraints_randomly (new_ts);
      } else {
	permute_npt (new_ts);
      }
    } else {
      assert (feasible (new_ts, FALSE) == new_ts->num_tasks);
    }

    /*
     * don't waste time unless new assignment is feasible
     */
    if (feasible (new_ts, FALSE) == new_ts->num_tasks) {

      if (target == -1 && target_pt_support) {
	maximize_preempt_thresholds (new_ts);
	assert (feasible (new_ts, FALSE) == new_ts->num_tasks);
      }
      
      new_thr = optimal_partition_into_threads (new_ts);
      assert (new_thr > 0 && new_thr <= new_ts->num_tasks);

      new_cs = find_critical_scale (new_ts, NULL);

      /*
       * don't waste a good result even if it's not what we're
       * currently looking for 
       */
      if (new_cs > best_cs_per_thr[new_thr]) {
	best_cs_per_thr[new_thr] = new_cs;
	if (best_ts_per_thr[new_thr]) free_task_set (best_ts_per_thr[new_thr]);
	best_ts_per_thr[new_thr] = copy_task_set (new_ts);
      }

#if 0 // FIXME!!!
      /*
       * if we ever see a "new" number of threads, go ahead and fork
       * off an annealing to maximize its insensitivity 
       */
      if (!optimized[new_thr]) {
	optimized[new_thr] = 1;
	anneal_threads_or_cs (copy_task_set(new_ts), 
			      new_thr, 
			      optimized, 
			      best_cs_per_thr,
			      best_ts_per_thr,
			      target_pt_support);
      }
#endif

      /*
       * annealing as usual, except we're optimizing two different
       * parameters depending on how we were called 
       */
      if ((target == -1 && (new_thr < best_thr || rand_double() < temp)) ||
	  (target != -1 && new_thr == target && (new_cs > best_cs || rand_double() < temp))) {
	last_improvement = i;
	best_thr = new_thr;
	best_cs = new_cs;
	free_task_set (best_ts);
	best_ts = copy_task_set (new_ts);
      }
    }

    free_task_set (new_ts);
    temp *= TEMP_SCALE;
    i++;

  }

  assert (best_cs <= best_cs_per_thr[best_thr]);

  assert (target == -1 || optimal_partition_into_threads (best_ts) == target);
  DBGPrint (1, ("  Done with %d.\n", target));

  free_task_set (best_ts);
}

void ensure_target_doesnt_need_pt (struct task_set *ts)
{
  assert (ts);
  assert (are_all_tasks_in_clusters (ts));
  if (requires_runtime_pt_support (ts)) {
    print_task_set (ts);
    assert (0);
  }
  assert (constraints_valid (ts));
}

void minimize_threads_by_annealing (struct task_set *ts1,
				    int test_overrun,
				    int target_pt_support)
{
  int *optimized;
  double *best_cs_per_thr;
  struct task_set **best_ts_per_thr;
  int i, ntasks;
  int least_threads, highest_cs;

  assert (ANNEAL_MAX != -1);

  assert (ts1);
  assert (uses_preempt_thresh_analysis (ts1));
  assert (ts1->Analysis.valid (ts1));
  if (!target_pt_support) {
    ensure_target_doesnt_need_pt (ts1);
  }

  DBGPrint (1, ("\n************* %s run-time PT support *************\n\n",
		(target_pt_support) ? "with" : "without"));

  ntasks = ts1->num_tasks;

  optimized = (int *) xmalloc (sizeof (int) * (1+ntasks));
  best_cs_per_thr = (double *) xmalloc (sizeof (double) * (1+ntasks));
  best_ts_per_thr = (struct task_set **) 
    xmalloc (sizeof (struct task_set *) * (1+ntasks));

  for (i=0; i<=ntasks; i++) {
    optimized[i] = FALSE;
    best_cs_per_thr[i] = 0;
    best_ts_per_thr[i] = NULL;
  }

  anneal_threads_or_cs (ts1, 
			-1,
			optimized, 
			best_cs_per_thr, 
			best_ts_per_thr, 
			target_pt_support);

  least_threads = highest_cs = -1;
  for (i=1; i<=ntasks; i++) {
    if (best_ts_per_thr[i] && least_threads == -1) least_threads = i;
    if (highest_cs == -1 || best_cs_per_thr[i] > best_cs_per_thr[highest_cs]) highest_cs = i;
  }
  
  for (i=1; i<=ntasks; i++) {
    DBGPrint (1, ("  for %d threads best cs is %f\n", i, best_cs_per_thr[i]));
  }
  
  assert (feasible (best_ts_per_thr[least_threads], TRUE) ==
	  best_ts_per_thr[least_threads]->num_tasks);
  DBGPrint (1, ("*** task set with fewest threads (cs = %f, threads = %d) ***\n",
		best_cs_per_thr[least_threads], least_threads));
  // print_task_set (best_ts_per_thr[least_threads]);
  // simulate (best_ts_per_thr[least_threads], 10000000, NULL, 0.0, NULL, stdout);
  
  printf ("%d ", least_threads);

  if (least_threads == highest_cs) {
    DBGPrint (1, ("*** task set with largest critical scaling factor is the same ***\n"));
  } else {
    assert (feasible (best_ts_per_thr[highest_cs], TRUE) ==
	    best_ts_per_thr[highest_cs]->num_tasks);
    DBGPrint (1, ("*** task set with largest critical scaling factor (cs = %f, threads = %d) ***\n",
		  best_cs_per_thr[highest_cs], highest_cs));
    // print_task_set (best_ts_per_thr[highest_cs]);
    // simulate (best_ts_per_thr[least_threads], 10000000, NULL, 0.0, NULL, stdout);
  }
  
  for (i=1; i<=ntasks; i++) {
    if (best_ts_per_thr[i]) {
      FILE *f;
      char fn[512];

      if (!target_pt_support) {
	ensure_target_doesnt_need_pt (best_ts_per_thr[i]);
      }

      sprintf (fn, "task_set_%d_threads_%s_rtpt.txt", 
	       i, (target_pt_support) ? "with" : "without"); 
      f = fopen (fn, "w");
      assert (f);
      fprint_task_set (best_ts_per_thr[i], f);
      fclose (f);

      if (test_overrun) {
	double frac;
	char str[MAX_NAMELEN];
	
	sprintf (str, "%d threads: ", i);
	for (frac=0.0; frac<1.0;  frac+=0.01) {
	  simulate (best_ts_per_thr[i], 10000000, NULL, frac, str, stdout);
	}
      }
      free_task_set (best_ts_per_thr[i]);
    }
  }

  xfree (optimized);
  xfree (best_cs_per_thr);
  xfree (best_ts_per_thr);
}

/*
 * Given a feasible assignment of priorities and thresholds, increase
 * the thresholds as much as possible without destroying schedulability.
 *
 * This is from Figure 4 of Saksena and Wang 00.
 */
void maximize_preempt_thresholds (struct task_set *ts)
{
  int i, j, rj;
  int cur_pri;

  assert (ts);
  assert (uses_preempt_thresh_analysis (ts));
  assert (ts->Analysis.valid (ts));  
  assert (no_zero_wcet (ts));

  for (cur_pri=0; cur_pri<ts->num_tasks; cur_pri++) {
    for (i=0; i<ts->num_tasks; i++) {
      if (ts->tasks[i].P == cur_pri) {
    	int schedulable = TRUE;
	    while (schedulable && ts->tasks[i].PT > 0) {
	      int k;
	      for (k=0; k<ts->num_task_barriers; k++) {
	        if (i > ts->task_barriers[k] &&
		         (ts->tasks[i].PT-1) <= ts->task_barriers[k]) {
	          schedulable = FALSE;
	          goto out;
	        }
	      }
	      DBGPrint (3, ("trying to raise PT of task %d to %d\n",
			i, ts->tasks[i].PT-1));
	      ts->tasks[i].PT--;
	      for (j=0; j<ts->num_tasks; j++) {
	        if (ts->tasks[j].P == ts->tasks[i].PT) {
	          rj = ts->Analysis.response_time (ts, j, 0);
	          if (rj > ts->tasks[j].D) {
		        assert (schedulable);
		        schedulable = FALSE;
		        ts->tasks[i].PT++;
		        goto out;
	          }
	        }
	      }
	      assert (feasible (ts, FALSE) == ts->num_tasks);
out:
	      ;
	    }
      }
    }
  }

  if (feasible (ts, FALSE) != ts->num_tasks) {
    print_task_set (ts);
    fflush (stdout);
    assert (0);
  }
}

enum which_energy { MAX = 0x233, SUM, SQUARED };

#define my_max(x,y) ((x)>(y)?(x):(y))

/*
 * This is the energy function for the simulated annealing algorithm
 * presented in Figure 2 of Saksena and Wang 00.  It is simply the
 * total lateness for all tasks for a given priority assignment, after
 * optimal preemption thresholds are assigned.  
 */
static time_value energy (struct task_set *ts)
{
  int i;
  // time_value e = 0;
  double e = 0;
  const enum which_energy which = SQUARED;
  
  for (i=0; i<ts->num_tasks; i++) {
    time_value pos_late = lateness (ts, i);
    switch (which) {
    case MAX:
      e = my_max (e, pos_late);
      break;
    case SUM:
      e += pos_late;
      break;
    case SQUARED:
      e += (double) pos_late * (double) pos_late;
      break;
    default:
      assert (0);
    }
  }
    
  if (which == SQUARED) e = sqrt (e);

  // printf ("%lld\n", e);

  return (time_value)e;
}

/*
 * Attempt to find a feasible assignment of priorities and preemption
 * thresholds using simulated annealing.
 *
 * This is very similar to the algorithm in Figure 2 of Saksena and
 * Wang 00.  
 */
int anneal_priorities_and_thresholds (struct task_set **ts,
				      int target_pt_support)
{
  int best_en, new_en;
  double temp = INIT_TEMP;
  struct task_set *new_ts, *best_ts;
  struct task_set *orig_ts;
  int i;
  int last_improvement = 0;
  int total_improvements = 0;

  assert (ANNEAL_MAX != -1);

  orig_ts = *ts;

  assert (ts);
  assert (orig_ts);
  assert (uses_preempt_thresh_analysis (orig_ts));

  best_ts = copy_task_set (orig_ts);
  free_task_set (orig_ts);

  if (target_pt_support) {
    set_priorities (best_ts, DM);
    make_all_preemptible (best_ts);
    respect_constraints_randomly (best_ts);
  } else {
    assert (are_all_tasks_in_clusters (best_ts));
    set_priorities (best_ts, BY_CLUSTER);
    set_preemption_thresholds_npt (best_ts);
    ensure_target_doesnt_need_pt (best_ts);
  }

  assert (best_ts->Analysis.valid (best_ts));

  best_en = energy (best_ts);

  i = 0;
  while (i < last_improvement + ANNEAL_MAX) {

    new_ts = copy_task_set (best_ts);

    if (target_pt_support) {
      permute_pri_and_thresh (new_ts);
      respect_constraints_randomly (new_ts);
    } else {
      permute_npt (new_ts);
    }

    new_en = energy (new_ts);

    if (i%100 == 0) {
      DBGPrint (3, ("%d : best = %d,  new = %d,  temp = %f\n", 
		    i, best_en, new_en, temp));
    }

    if (new_en == 0) {
      *ts = new_ts;
      free_task_set (best_ts);
      if (!target_pt_support) {
	ensure_target_doesnt_need_pt (new_ts);
      }
      assert (feasible (new_ts, TRUE) == num_tasks (new_ts));
      return TRUE;
    }
    
    if ((new_en <= best_en ||
	 rand_double() < temp)) {
      if (new_en < best_en) {
	last_improvement = i;
	total_improvements++;
      }
      best_en = new_en;
      free_task_set (best_ts);
      best_ts = copy_task_set (new_ts);
    }

    free_task_set (new_ts);
    temp *= TEMP_SCALE;
    i++;

  }

  DBGPrint (3, ("anneal_priorities_and_thresholds failed after %d tries; final temp %f\n", 
		i, temp));

  feasible (best_ts, TRUE);

  *ts = best_ts;
  return FALSE;
}

#ifdef USE_DVS
freq_level get_task_frequency_level(struct task_set *ts, int t)
{
	assert (ts);
	assert (t < num_tasks(ts));

	return ts->tasks[t].f;
}

void set_task_frequency_level(struct task_set *ts, int t, freq_level f)
{
	time_value C = 0;

	assert (ts);
	assert (t < num_tasks(ts));
	assert (f >= MIN_FREQ_LEVEL);
	assert (f <= MAX_FREQ_LEVEL);

	DBGPrint(3,("change %s frequency level from %d to %d.\n",
				ts->tasks[t].name,
				ts->tasks[t].f,
				f));

	if(get_task_frequency_level(ts,t) == f)
		return;

	C = modify_task_C_by_freq(get_Cu(ts, t), valid_f_scale[f]);
	set_wcet(ts,t,C);
	ts->tasks[t].f = f;
}

void inc_task_frequency_level(struct task_set *ts, int t)
{
	assert (ts);
	assert (t < num_tasks(ts));

	freq_level f = get_task_frequency_level(ts,t);
	f += (f < MAX_FREQ_LEVEL)? 1: 0;
	set_task_frequency_level(ts,t,f);
}

void dec_task_frequency_level(struct task_set *ts, int t)
{
	assert (ts);
	assert (t < num_tasks(ts));

	freq_level f = get_task_frequency_level(ts,t);
	f -= (f!=0)? 1: 0;
	set_task_frequency_level(ts,t,f);
}

freq_level get_task_set_frequency_level(struct task_set *ts)
{
	assert (ts);
	assert (ts->tasks);

	return ts->tasks[0].f;
}

void set_task_set_frequency_level(struct task_set *ts, freq_level f)
{
	assert (ts);
	assert (f >= MIN_FREQ_LEVEL);
	assert (f <= MAX_FREQ_LEVEL);

	if(get_task_set_frequency_level(ts) == f)
		return;

	int i = num_tasks(ts)-1;

	for(;i >=0; i--){
		set_task_frequency_level(ts,i,f);
	}
}

void inc_task_set_frequency_level(struct task_set *ts)
{
	assert (ts);

	freq_level f = get_task_set_frequency_level(ts);
	f += (f < MAX_FREQ_LEVEL)? 1: 0;
	set_task_set_frequency_level(ts,f);
}

void dec_task_set_frequency_level(struct task_set *ts)
{
	assert (ts);

	freq_level f = get_task_set_frequency_level(ts);
	f -= (f > MIN_FREQ_LEVEL)? 1: 0;
	set_task_set_frequency_level(ts,f);
}
#endif
