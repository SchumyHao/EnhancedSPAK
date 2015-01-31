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

static int sched_scale (struct task_set *ts,
			time_value *Corig,
			double scale)
{
  int i;

  DBGPrint (5, ("sched_scale called with %9.9f\n", scale));
 
  for (i=0; i<ts->num_tasks; i++) {
    ts->tasks[i].C = (time_value) (Corig[i] * scale);
  }

  if (feasible (ts, FALSE) == ts->num_tasks) {
    DBGPrint (5, ("  returning TRUE\n"));
    return 1;
  } else {
    DBGPrint (5, ("  returning FALSE\n"));
    return 0;
  }
}

#define THRESH 0.00001

double find_critical_scale (struct task_set *ts,
			    struct task_set **ts2)
{
  time_value *Corig;
  int i;
  double low, high;

  // printf ("entering find_critical_scale\n");

  Corig = (time_value *) xmalloc (ts->num_tasks * sizeof (time_value));
  for (i=0; i<ts->num_tasks; i++) {
    Corig[i] = ts->tasks[i].C;
  }

  /*
   * find a point that is schedulable and one that isn't
   */
  if (sched_scale (ts, Corig, 1.0)) {
    low = 1.0;
    high = 1.0;
  here1:
    high *= 10.0;
    if (sched_scale (ts, Corig, high)) {
      goto here1;
    }
  } else {
    high = 1.0;
    low = 1.0;
  here2:
    low *= 0.1;
    if (!sched_scale (ts, Corig, low)) {
      goto here2;
    }
  }

  assert (!sched_scale (ts, Corig, high));
  assert (sched_scale (ts, Corig, low));

  /*
   * binary search -- this requires that the schedulability function
   * be monotonic 
   *
   * the loop invariant is that sched_scale (ts, Corig, low) is always
   * true 
   */
  do {
    double middle = (low + high) / 2;

    DBGPrint (5, ("low = %9.9f, middle = %9.9f, high = %9.9f\n", low, middle, high));

    if (sched_scale (ts, Corig, middle)) {
      low = middle;
    } else {
      high = middle;
    }

    assert (sched_scale (ts, Corig, low));

  } while ((high - low) > THRESH);

  assert (sched_scale (ts, Corig, low));

  if (ts2) {
    *ts2 = copy_task_set (ts);
  }

  for (i=0; i<ts->num_tasks; i++) {
    ts->tasks[i].C = Corig[i];
  }

  xfree (Corig);

  // printf ("exiting find_critical_scale\n");

  return low;
}

static void apply_scale (double scale, 
			 struct task_set *orig,
			 struct task_set *scaled)
{
  int i;

  assert (orig->num_tasks == scaled->num_tasks);
  
  for (i=0; i < orig->num_tasks; i++) {
    scaled->tasks[i].C = scale * orig->tasks[i].C;
  }
}

struct task_set *maximize_insensitivity_optimal (struct task_set *ts_in)
{
  double low, high;
  struct task_set *ts;

  assert (ts_in);
  assert (ts_in->Analysis.valid (ts_in));

  ts = copy_task_set (ts_in);
  
  // print_task_set (ts);

  /*
   * don't use this for mixed-mode task sets; they have no optimal
   * algorithm with quadratic run time
   */
  assert (is_all_preemptible (ts) || is_all_nonpreemptible (ts));

  assert (feasible (ts, FALSE) == ts->num_tasks);

  low = 1.0;
  // high = 1.0 / utilization_set (ts);
  high = 30;
  
  do {
    int res;
    double middle;

    middle = (low + high) / 2.0;

    apply_scale (middle, ts_in, ts);
    res = assign_optimal_pri (ts);

    DBGPrint (5, ("high = %f, low = %f, middle = %f, res = %d\n",
		  high, low, middle, res));

    if (res) {
      low = middle;
    } else {
      high = middle;
    }    
  } while ((high - low) > THRESH);

  apply_scale (low, ts_in, ts);
  {
    int res = assign_optimal_pri (ts);
    assert (res);
  }
  apply_scale (1.0, ts_in, ts);

  return ts;
}

static int test_critical_scale (struct task_set *ts,
				double test_value)
{
  time_value *Corig;
  int res, i;

  Corig = (time_value *) xmalloc (ts->num_tasks * sizeof (time_value));
  for (i=0; i<ts->num_tasks; i++) {
    Corig[i] = ts->tasks[i].C;
  }

  res = sched_scale (ts, Corig, test_value);
        
  for (i=0; i<ts->num_tasks; i++) {
    ts->tasks[i].C = Corig[i];
  }

  xfree (Corig);

  return res;
}

static void permute_pri_once (struct task_set *ts)
{
  int t = rand_long() % ts->num_tasks;

  if (rand_double() < 0.5) {
    int new_pri = rand_long() % ts->num_tasks;
    assign_pri (ts, t, new_pri);
  } else {
    if (rand_double() < 0.5) {
      if (ts->tasks[t].P < ts->num_tasks-1) assign_pri (ts, t, ts->tasks[t].P+1);
    } else {
      if (ts->tasks[t].P > 0) assign_pri (ts, t, ts->tasks[t].P-1);
    }    
  }
}

static void permute_thresh_once (struct task_set *ts)
{
  int t = rand_long() % ts->num_tasks;
  if (rand_double() < 0.5) {
    int new_pt = rand_long() % ts->num_tasks;
    ts->tasks[t].PT = new_pt;
  } else {
    if (rand_double() < 0.5) {
      ts->tasks[t].PT++;
    } else {
      ts->tasks[t].PT--;
    }
  }
}

void permute_pri (struct task_set *ts)
{
  do {
    permute_pri_once (ts);
  } while (rand_double() < 0.4);
}

void permute_pri_and_thresh (struct task_set *ts)
{
  int t;

  do {
    if (rand_double() < 0.5) {
      permute_pri_once (ts);
    } else {
      permute_thresh_once (ts);
    }
  } while (rand_double() < 0.4);

  for (t=0; t<ts->num_tasks; t++) {
    if (ts->tasks[t].PT > ts->tasks[t].P ||
	ts->tasks[t].PT < 0) {
      if (ts->tasks[t].P == 0) {
	ts->tasks[t].PT = 0;
      } else {
	ts->tasks[t].PT = rand_long()%ts->tasks[t].P;
      }
    } 
    assert (ts->tasks[t].PT >= 0 && 
	    ts->tasks[t].PT < ts->num_tasks);
    assert (ts->tasks[t].PT <= ts->tasks[t].P);
  }
}

void randomize_priorities (struct task_set *ts)
{
  int i;

  assert (ts);

  for (i=0; i<ts->num_tasks; i++) {
    permute_pri_once (ts);
  }
  make_all_preemptible (ts);
}

static int can_swap_clusters (struct task_set *ts, int c1, int c2)
{
  int ex1 = ts->task_clusters[c1].tasks[0];
  int ex2 = ts->task_clusters[c2].tasks[0];
  int i;

  for (i=0; i<ts->num_task_barriers; i++) {
    int b = ts->task_barriers[i];
    if (b < ts->tasks[ex1].P &&
	b >= ts->tasks[ex2].P) return FALSE;
    if (b >= ts->tasks[ex1].P &&
	b < ts->tasks[ex2].P) return FALSE;
  }
  return TRUE;
}

void set_preemption_thresholds_npt (struct task_set *ts)
{
  int i;
  for (i=0; i<ts->num_task_clusters; i++) {
    int maxp = ts->num_tasks+1;
    int j;
    for (j=0; j<ts->task_clusters[i].num_tasks; j++) {
      int t = ts->task_clusters[i].tasks[j];
      if (ts->tasks[t].P < maxp) {
	maxp = ts->tasks[t].P;
      }
    }
    assert (maxp != ts->num_tasks+1);
    for (j=0; j<ts->task_clusters[i].num_tasks; j++) {
      int t = ts->task_clusters[i].tasks[j];
      ts->tasks[t].PT = maxp;
    }
  }
}

static void swap_clusters (struct task_set *ts, int c1, int c2)
{
  int p1, p2, i, inc1, inc2;

  if (minpri (ts, c1) < minpri (ts, c2)) {
    p1 = maxpri (ts, c1);
    inc1 = 1;
    p2 = minpri (ts, c2);
    inc2 = -1;
  } else {
    p1 = minpri (ts, c1);
    inc1 = -1;
    p2 = maxpri (ts, c2);
    inc2 = 1;
  }

  for (i=0; i<ts->task_clusters[c1].num_tasks; i++) {
    int t = ts->task_clusters[c1].tasks[i];
    DBGPrint (4, ("  assigning task %d to pri %d\n", t, p2));
    assign_pri (ts, t, p2);
    p2 += inc2;
  }
  for (i=0; i<ts->task_clusters[c2].num_tasks; i++) {
    int t = ts->task_clusters[c2].tasks[i];
    DBGPrint (4, ("  assigning task %d to pri %d\n", t, p2));
    assign_pri (ts, t, p1);
    p1 += inc1;
  }
}

#ifdef JOIN_CLUSTERS

static int are_clusters_adjascent (struct task_set *ts, int i, int j)
{
  if ((ts->tasks[ts->task_clusters[j].tasks[0]].PT + 
       ts->task_clusters[j].num_tasks) ==
      ts->tasks[ts->task_clusters[i].tasks[0]].PT) {
    return TRUE;
  } else {
    return FALSE;
  }
}

int are_clusters_joinable (struct task_set *ts, int i, int j)
{
  if (are_clusters_adjascent (ts, i, j) && can_swap_clusters (ts, i, j)) {
    return TRUE;
  } else {
    return FALSE;
  }
}

static void join_clusters (struct task_set *ts, int i, int j)
{
  int k;
  for (k=0; k<ts->task_clusters[i].num_tasks; k++) {
    int t = ts->task_clusters[i].tasks[k];
    ts->tasks[t].PT = ts->tasks[ts->task_clusters[j].tasks[0]].PT;
  }
}

static void set_preemption_thresholds_npt_joined (struct task_set *ts)
{
  int x, i, j;
  for (x=0; x<ts->num_task_clusters; x++) {
    for (i=0; i<ts->num_task_clusters; i++) {
      if (!ts->task_clusters[i].merge) continue;
      for (j=0; j<ts->num_task_clusters; j++) {
	if (are_clusters_joinable (ts, i, j)) {
	  join_clusters (ts, i, j);
	}
      }
    }
  }
}

#endif

void permute_npt (struct task_set *ts)
{
  assert (!requires_runtime_pt_support (ts));

  /*
   * FIXME -- we are ignoring the possibility of joining clusters
   * (i.e. setting the PT of adjascent clusters to the max PT of
   * either cluster) also, tasks in joined clusters can be swapped
   * around, but this makes it hard to reseparate the clusters --
   * maybe joining should only be done greedily 
   */
  if (rand_double() < 0.5) {
    // permute tasks within a cluster
    int c, i1, i2, t1, t2, tmp, z;
    z = 0;
    do {
      c = rand_long() % ts->num_task_clusters;
      if (z++ > 100) goto out; // stupid hack
    } while (ts->task_clusters[c].num_tasks == 1);
    i1 = rand_long() % ts->task_clusters[c].num_tasks;
    i2 = rand_long() % ts->task_clusters[c].num_tasks;
    t1 = ts->task_clusters[c].tasks[i1];
    t2 = ts->task_clusters[c].tasks[i2];
    DBGPrint (4, ("swap priorities of %d and %d in cluster %d\n",
		  t1, t2, c));
    // print_cluster (ts, c);
    // printf ("t1 = %d, t2 = %d\n", t1, t2);
    assert (ts->tasks[t1].PT == ts->tasks[t2].PT);
    tmp = ts->tasks[t1].P;
    ts->tasks[t1].P = ts->tasks[t2].P;
    ts->tasks[t2].P = tmp;
  } else {
    /*
     * permute clusters -- need to pick clusters that are not
     * separated by a barrier 
     */
    int c1, c2; // index of clusters 1 and 2
    int z = 0;
    do {
      // FIXME -- broken when there are no clusters that can be switched
      c1 = rand_long() % ts->num_task_clusters;
      c2 = rand_long() % ts->num_task_clusters;
      if (z++> 250) goto out;
    } while (!can_swap_clusters (ts, c1, c2));
    DBGPrint (4, ("-> swap clusters %d %d\n", c1, c2));
    swap_clusters (ts, c1, c2);
    set_preemption_thresholds_npt (ts);
  }

 out:
  ;

#ifdef JOIN_CLUSTERS
  if (rand_double() < 0.5) {
    if (rand_double() < 0.65) {
      ts->task_clusters[rand_long()%ts->num_task_clusters].merge = 0;
    } else {
      ts->task_clusters[rand_long()%ts->num_task_clusters].merge = 1;      
    }
  }
  set_preemption_thresholds_npt_joined (ts);
#endif

  if (requires_runtime_pt_support (ts)) {
    print_task_set (ts);
    assert (0);
  }
}

struct task_set *maximize_insensitivity_by_annealing (struct task_set *orig_ts,
						      int PT,
						      int target_pt_support)
{
  double best_bd, new_bd;
  double temp = INIT_TEMP;
  struct task_set *new_ts, *best_ts;
  int i;
  int last_improvement = 0;
  int total_improvements = 0;
  int preemptible = -1;

  assert (ANNEAL_MAX != -1);

  assert (orig_ts);
  assert (uses_preempt_thresh_analysis (orig_ts));
  assert (orig_ts->Analysis.valid (orig_ts));

  if (!target_pt_support) {
    assert (are_all_tasks_in_clusters (orig_ts));
  } else {
    respect_constraints_randomly (orig_ts);    
  }

  DBGPrint (3, ("maximize_insensitivity_by_annealing (pt=%s, runtime pt=%s) : \n",
		(PT) ? "yes" : "no", 
		(target_pt_support) ? "yes" : "no"));

  DBGPrint (3, ("INIT_TEMP = %f, TEMP_SCALE = %f\n", INIT_TEMP, TEMP_SCALE));

  if (!PT) {
    assert (!target_pt_support);
    if (is_all_preemptible (orig_ts)) {
      preemptible = TRUE;
    } else if (is_all_nonpreemptible (orig_ts)) {
      preemptible = FALSE;
    } else {
      assert (0);
    }
  }

  best_bd = find_critical_scale (orig_ts, NULL);
  best_ts = copy_task_set (orig_ts);
  free_task_set (orig_ts);

  i = 0;
  while (i < last_improvement + ANNEAL_MAX) {

    new_ts = copy_task_set (best_ts);
    if (PT) {
      if (target_pt_support) {
	permute_pri_and_thresh (new_ts);
	respect_constraints_randomly (new_ts);
      } else {
	permute_npt (new_ts);
      }
    } else {
      permute_pri (new_ts);
      if (preemptible) {
	make_all_preemptible (new_ts);
      } else {
	make_all_nonpreemptible (new_ts);
      }
    }
    
    if (i%100 == 0) {
      DBGPrint (3, ("  anneal rep %d; best is %f; temp is %f\n", 
		    i, best_bd, temp));
    }

    if (feasible (new_ts, FALSE) == new_ts->num_tasks) {

      if (test_critical_scale (new_ts, best_bd) ||
	  (rand_double() < temp &&
	   test_critical_scale (new_ts, (1+((best_bd-1.0)/2))))) {
	new_bd = find_critical_scale (new_ts, NULL);
	if (new_bd > best_bd) {
	  last_improvement = i;
	  total_improvements++;
	}
	assert (new_bd >= 1.0);
	best_bd = new_bd;
	free_task_set (best_ts);
	best_ts = copy_task_set (new_ts);
      }
    }
    
    free_task_set (new_ts);
    temp *= TEMP_SCALE;
    i++;
    
  }

  DBGPrint (3, ("  tested %d;  final temp is %f; last imp. at %d, total imp. %d\n", 
		i, temp, last_improvement, total_improvements));

  return best_ts;
}

struct task_set *greedy (struct task_set *ts)
{
  double best_bd, new_bd;
  struct task_set *best_ts, *new_ts;
  int count = 0;
  int tested = 0;

  best_bd = find_critical_scale (ts, NULL);
  best_ts = copy_task_set (ts);
  free_task_set (ts);

  do {

    new_ts = copy_task_set (best_ts);
    permute_pri (new_ts);
    new_bd = find_critical_scale (new_ts, NULL);
    count++;
    tested++;

    if (new_bd > best_bd) {
      best_bd = new_bd;
      free_task_set (best_ts);
      best_ts = copy_task_set (new_ts);
      count = 0;
    }
    
    free_task_set (new_ts);

  } while (count < 20000);

  printf ("tested %d\n", tested);
  
  return best_ts;
}

static int total;
static double best_bd;
static struct task_set *best_ts;

static void test (struct task_set *ts)
{
  double new_bd;

  new_bd = find_critical_scale (ts, NULL);
  if (new_bd > best_bd) {
    best_bd = new_bd;
    if (best_ts) {
      free_task_set (best_ts);
    }
    best_ts = copy_task_set (ts);
  }
}

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
      // all priorities assigned
      total++;
      test (ts);
    } else {
      // still more tasks to go
      pick_pri (ts, t+1);
    } 
  loop_end:
    ;
  }
}

struct task_set *exhaustive (struct task_set *ts)
{
  total = 0;
  best_bd = -1;
  best_ts = NULL;
  pick_pri (ts, 0);
  free_task_set (ts);
  printf ("tested %d\n", total);
  return best_ts;
}

#ifdef XMALLOC_CNT
int xmalloc_cnt = 0;
#endif

void print_xmalloc_cnt (void)
{
#ifdef XMALLOC_CNT
  printf ("malloc count = %d\n", xmalloc_cnt);
#endif
}
