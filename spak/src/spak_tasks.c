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

time_value max_resp = -1;

void spak_srand (void)
{
    int seed;

#ifdef WIN32
    seed = time (NULL);
#else
    seed = time (NULL) * getpid ();
#endif

    seed_rand (seed);
    DBGPrint (3, ("seed = %d\n", seed));
}

time_value get_period (struct task_set* ts, int t)
{
    return ts->tasks[t].T;
}
#ifdef USE_CLUSTER
void set_jitter (struct task_set* ts, int t, int J)
{
    assert (ts);
    ts->tasks[t].J = J;
}
#endif
void change_wcet (struct task_set* ts, int t, int inc)
{
    assert (ts);
    ts->tasks[t].C += inc;
}

void set_wcet (struct task_set* ts, int t, time_value newC)
{
    assert (ts);
    ts->tasks[t].C = newC;
}

time_value get_wcet (struct task_set* ts, int t)
{
    assert (ts);
    return ts->tasks[t].C;
}

int internal_set_analysis (struct spak_analysis* A, const char* which)
{
    // relies on short-circuit behavior
    if (!get_analysis1_ptrs (which, A) &&
        !get_analysis2_ptrs (which, A) &&
        !get_analysis3_ptrs (which, A) &&
        !get_analysis4_ptrs (which, A) &&
        !get_analysis5_ptrs (which, A) &&
        !get_analysis6_ptrs (which, A)
#ifdef USE_DVS
        && !get_analysis7_ptrs (which, A)
#endif
       ) {
        printf ("oops: invalid analysis %s specified\n", which);
        assert (0);
    }

    return 1;
}

void set_analysis (struct task_set* ts, const char* which)
{
    assert (ts);
    assert (which);

    internal_set_analysis (&ts->Analysis, which);
}

/*
 * create a task set
 */
struct task_set* create_task_set (int max_tasks,
                                  int max_sems,
                                  int max_locks,
                                  int max_clusters,
                                  const char* name,
                                  time_value Tclk,
                                  time_value Cclk,
                                  time_value Cql,
                                  time_value Cqs,
                                  const char* which)
{
    struct task_set* ts = (struct task_set*) xmalloc (sizeof (struct task_set));

    assert (max_tasks > 0);
    assert (max_sems >= 0);
    assert (max_locks >= 0);
    assert (name);
    assert (Tclk > 0);
    assert (Cclk >= 0);
    assert (Cql >= 0);
    assert (Cqs >= 0);

    internal_set_analysis (&ts->Analysis, which);

    ts->max_tasks = max_tasks;
    ts->num_tasks = 0;
    ts->tasks = (struct task*) xmalloc (max_tasks * sizeof (struct task));

    ts->max_sems = max_sems;
    ts->num_sems = 0;
    if (max_sems > 0) {
        ts->sems = (struct sem*) xmalloc (max_sems * sizeof (struct sem));
    }
    else {
        ts->sems = NULL;
    }

    ts->max_locks = max_locks;
    ts->num_locks = 0;
    if (max_locks > 0) {
        ts->locks = (struct lock*) xmalloc (max_locks * sizeof (struct lock));
    }
    else {
        ts->locks = NULL;
    }

    ts->max_task_clusters = max_clusters;
    ts->num_task_clusters = 0;
    if (max_clusters > 0) {
        size_t size = max_clusters * sizeof (struct task_cluster);
        ts->task_clusters = (struct task_cluster*) xmalloc (size);
    }
    else {
        ts->task_clusters = NULL;
    }

    ts->num_task_barriers = 0;

    strncpy (ts->name, name, MAX_NAMELEN);

    ts->Tclk = Tclk;
    ts->Cclk = Cclk;
    ts->Cql = Cql;
    ts->Cqs = Cqs;

    return ts;
}

struct task_set* copy_task_set (struct task_set* ts1)
{
    struct task_set* ts2;

    assert (ts1);

    ts2 = (struct task_set*) xmalloc (sizeof (struct task_set));
    *ts2 = *ts1;

    {
        unsigned int size = ts1->max_tasks * sizeof (struct task);
        ts2->tasks = (struct task*) xmalloc (size);
        assert (ts1->tasks);
        memcpy (ts2->tasks, ts1->tasks, size);
    }

    if (ts1->max_sems > 0) {
        unsigned int size = ts1->max_sems * sizeof (struct sem);
        ts2->sems = (struct sem*) xmalloc (size);
        assert (ts1->sems);
        memcpy (ts2->sems, ts1->sems, size);
    }

    if (ts1->max_locks > 0) {
        unsigned int size = ts1->max_locks * sizeof (struct lock);
        ts2->locks = (struct lock*) xmalloc (size);
        assert (ts1->locks);
        memcpy (ts2->locks, ts1->locks, size);
    }

    if (ts1->max_task_clusters > 0) {
        unsigned int size = ts1->max_task_clusters * sizeof (struct task_cluster);
        ts2->task_clusters = (struct task_cluster*) xmalloc (size);
        assert (ts1->task_clusters);
        memcpy (ts2->task_clusters, ts1->task_clusters, size);
    }

    return ts2;
}

void free_task_set (struct task_set* ts)
{
    assert (ts);
    assert (ts->tasks);

    xfree (ts->tasks);
    if (ts->sems) xfree (ts->sems);
    if (ts->locks) xfree (ts->locks);
    if (ts->task_clusters) xfree (ts->task_clusters);
    xfree (ts);
}

void make_all_preemptible (struct task_set* ts)
{
    int i;

    assert (ts);

    for (i=0; i<ts->num_tasks; i++) {
        ts->tasks[i].PT = ts->tasks[i].P;
    }
}

void make_all_nonpreemptible (struct task_set* ts)
{
    int i, min;

    assert (ts);

    min = ts->num_tasks;
    for (i=0; i<ts->num_tasks; i++) {
        if (ts->tasks[i].P < min) {
            min = ts->tasks[i].P;
        }
    }

    for (i=0; i<ts->num_tasks; i++) {
        ts->tasks[i].PT = min;
    }
}

int is_all_preemptible (struct task_set* ts)
{
    int i;

    assert (ts);

    for (i=0; i<ts->num_tasks; i++) {
        if (ts->tasks[i].P != ts->tasks[i].PT) return FALSE;
    }

    return TRUE;
}

int is_all_nonpreemptible (struct task_set* ts)
{
    int i, min;

    assert (ts);

    min = ts->num_tasks+1;
    for (i=0; i<ts->num_tasks; i++) {
        if (ts->tasks[i].P < min) {
            min = ts->tasks[i].P;
        }
    }

    for (i=0; i<ts->num_tasks; i++) {
        if (ts->tasks[i].PT > min) return FALSE;
    }

    return TRUE;
}
#ifdef USE_CLUSTER
void new_task_barrier (struct task_set* ts, int x)
{
    assert (ts);
    assert (x >= 0 && x < ts->num_tasks);

    if (ts->num_task_barriers >= MAX_BARRIERS-1) {
        printf ("oops: too many preemption threshold barriers\n");
        assert (0);
    }

    ts->task_barriers[ts->num_task_barriers] = x;
    ts->num_task_barriers++;
}

struct task_cluster* new_task_cluster (struct task_set* ts,
                                       const char* name)
{
    int n;
    struct task_cluster* cluster;

    assert (ts);
    assert (ts->task_clusters);

    n = ts->num_task_clusters;

    if (n == ts->max_task_clusters) {
        printf ("oops: too many preemption constraints\n");
        assert (0);
    }

    cluster = &ts->task_clusters[n];
    cluster->num_tasks = 0;
    cluster->merge = 0;
    strncpy (cluster->name, name, MAX_NAMELEN-1);

    ts->num_task_clusters++;

    DBGPrint (5, ("there are now %d (out of %d) task clusters\n",
                  ts->num_task_clusters, ts->max_task_clusters));

    return cluster;
}

void add_to_task_cluster (struct task_set* ts,
                          struct task_cluster* cluster,
                          const char* task)
{
    int n;
    int task_num;
    int i;

    assert (ts);
    assert (cluster);

    // insanity checks
    for (i=0; i<ts->num_task_clusters; i++) {
        if (&ts->task_clusters[i] == cluster) goto out;
    }
    assert (0);
out:
    for (i=0; i<cluster->num_tasks; i++) {
        assert (strcmp (task, ts->tasks[cluster->tasks[i]].name) != 0);
    }

    task_num = -1;
    for (i=0; i<ts->num_tasks; i++) {
        if (strcmp (ts->tasks[i].name, task) == 0) {
            task_num = i;
        }
    }
    assert (task_num != -1);

    n = cluster->num_tasks;
    if (n+1 == MAX_TASKS_PER_CLUSTER) {
        printf ("oops: too many tasks in NP group %s\n",
                cluster->name);
        assert (0);
    }

    cluster->tasks[n] = task_num;

    cluster->num_tasks++;
}

int in_cluster (struct task_set* ts, int t)
{
    int i, j, x = 0;

    for (i=0; i<ts->num_task_clusters; i++) {
        for (j=0; j<ts->task_clusters[i].num_tasks; j++) {
            if (ts->task_clusters[i].tasks[j] == t) {
                x++;
            }
        }
    }

    if (x == 0) return FALSE;
    if (x == 1) return TRUE;
    assert (0);
    return FALSE;
}

void create_random_task_cluster (struct task_set* ts, int num)
{
    int i, t;
    struct task_cluster* c;

    printf ("task set with %d members will have a cluster with %d members\n",
            ts->num_tasks, num);

    c = new_task_cluster (ts, "random cluster");

    for (i=0; i<num; i++) {
        do {
            t = rand_long()%ts->num_tasks;
        }
        while (in_cluster (ts, t));
        add_to_task_cluster (ts, c, ts->tasks[t].name);
    }
}

void create_random_task_barrier (struct task_set* ts)
{
    // FIXME: makes no attempt to avoid an existing barrier
    int n = rand_long() % (ts->num_tasks-1);
    new_task_barrier (ts, n);
}

void put_all_tasks_in_one_cluster (struct task_set* ts)
{
    struct task_cluster* tc = new_task_cluster (ts, "all");
    int i;

    for (i=0; i<ts->num_tasks; i++) {
        add_to_task_cluster (ts, tc, ts->tasks[i].name);
    }

    assert (are_all_tasks_in_clusters (ts));
}

int has_jitter (struct task_set* ts)
{
    int i;
    for (i=0; i<ts->num_tasks; i++) {
        if (ts->tasks[i].J) return TRUE;
    }
    return FALSE;
}

int has_task_barriers (struct task_set* ts)
{
    return ((ts->num_task_barriers > 0) ? TRUE : FALSE);
}

int has_task_clusters (struct task_set* ts)
{
    int i;
    for (i=0; i<ts->num_task_clusters; i++) {
        if (ts->task_clusters[i].num_tasks > 1) return TRUE;
    }
    return FALSE;
}

int has_constraints (struct task_set* ts)
{
    return ((has_task_barriers (ts) || has_task_clusters (ts)) ? TRUE : FALSE);
}

/*
 * max is confusing here: we return the highest priority or
 * threshold, which numerically is lowest
 */
static int maxp_pt (struct task_set* ts, int c)
{
    int max = ts->num_tasks+1;
    int i, p, pt;

    for (i=0; i<ts->task_clusters[c].num_tasks; i++) {
        p = ts->tasks[ts->task_clusters[c].tasks[i]].P;
        pt = ts->tasks[ts->task_clusters[c].tasks[i]].PT;
        if (p < max) max = p;
        if (pt < max) max = pt;
    }

    return max;
}

static int minp_pt (struct task_set* ts, int c)
{
    int min = 0;
    int i, p, pt;

    for (i=0; i<ts->task_clusters[c].num_tasks; i++) {
        p = ts->tasks[ts->task_clusters[c].tasks[i]].P;
        pt = ts->tasks[ts->task_clusters[c].tasks[i]].PT;
        if (p > min) min = p;
        if (pt > min) min = pt;
    }

    return min;
}

static int is_constraint_conflict (struct task_set* ts)
{
    int i;

    for (i=0; i<ts->num_task_clusters; i++) {
        int j;
        int max_p = maxp_pt (ts, i);
        int min_p = minp_pt (ts, i);
        for (j=0; j<ts->num_task_barriers; j++) {
            if (ts->task_barriers[j] >= max_p && ts->task_barriers[j] < min_p) {
                return TRUE;
            }
        }
    }

    return FALSE;
}

int barriers_permit_pri (struct task_set* ts, int t, int pri)
{
    int j;
    for (j=0; j<ts->num_task_barriers; j++) {
        int b = ts->task_barriers[j];
        if (b >= t && pri > b) return FALSE;
        if (b < t && pri <= b) return FALSE;
    }

    return TRUE;
}
#endif
int is_pri_unique (struct task_set* ts)
{
    int* x = (int*) xmalloc ((sizeof (int) * ts->num_tasks));
    int i;

    for (i=0; i<ts->num_tasks; i++) {
        x[i] = FALSE;
    }

    for (i=0; i<ts->num_tasks; i++) {
        int P = ts->tasks[i].P;
        if (P < 0 || P >= ts->num_tasks || x[P]) {
            xfree (x);
            return FALSE;
        }
        x[P] = TRUE;
    }

    xfree (x);
    return TRUE;
}
#ifdef USE_CLUSTER
int constraints_valid (struct task_set* ts)
{
    {
        int i, j, k;

        for (i=0; i<ts->num_tasks; i++) {
            if (ts->tasks[i].P < 0 || ts->tasks[i].P >= ts->num_tasks) return FALSE;
            if (ts->tasks[i].PT < 0 || ts->tasks[i].PT >= ts->num_tasks) return FALSE;
        }

        if (is_constraint_conflict (ts)) return FALSE;

        for (i=0; i<ts->num_task_clusters; i++) {
            struct task_cluster* cluster = &ts->task_clusters[i];
            assert (ts->task_clusters[i].num_tasks > 0 &&
                    ts->task_clusters[i].num_tasks <= ts->num_tasks);
            for (j=0; j<cluster->num_tasks; j++) {
                int tj = cluster->tasks[j];
                for (k=j+1; k<cluster->num_tasks; k++) {
                    int tk = cluster->tasks[k];
                    if (ts->tasks[tj].P < ts->tasks[tk].PT ||
                        ts->tasks[tk].P < ts->tasks[tj].PT) {
                        DBGPrint (1, ("oops: tasks %d and %d from group %s aren't nonpreemptible\n",
                                      tj, tk, cluster->name));
                        return FALSE;
                    }
                }
            }
        }
    }

    {
        int z;

        for (z=0; z<ts->num_task_barriers; z++) {
            int bi = ts->task_barriers[z];
            int j;
            for (j=0; j<ts->num_tasks; j++) {
                if (j>bi && (ts->tasks[j].P <= bi || ts->tasks[j].PT <= bi)) return FALSE;
                if (j<=bi && (ts->tasks[j].P > bi || ts->tasks[j].PT > bi)) return FALSE;
            }
        }
    }

    return TRUE;
}

static void respect_task_barriers (struct task_set* ts, int* change)
{
    int z, j;

    for (z=0; z<ts->num_task_barriers; z++) {
        int bi = ts->task_barriers[z];
        for (j=0; j<ts->num_tasks; j++) {
            if (j > bi && ts->tasks[j].P <= bi) {
                DBGPrint (5, ("respect_task_barriers: task %d pri from %d to %d\n",
                              j, ts->tasks[j].P, bi+1));
                *change = TRUE;
                // ts->tasks[j].P = bi+1;
                assign_pri (ts, j, bi+1);
            }
            if (j > bi && ts->tasks[j].PT <= bi) {
                DBGPrint (5, ("respect_task_barriers: task %d PT from %d to %d\n",
                              j, ts->tasks[j].P, bi+1));
                *change = TRUE;
                ts->tasks[j].PT = bi+1;
            }
            if (j <= bi && ts->tasks[j].P > bi) {
                DBGPrint (5, ("respect_task_barriers: task %d pri from %d to %d\n",
                              j, ts->tasks[j].P, bi));
                *change = TRUE;
                // ts->tasks[j].P = bi;
                assign_pri (ts, j, bi);
            }
            if (j <= bi && ts->tasks[j].PT > bi) {
                DBGPrint (5, ("respect_task_barriers: task %d PT from %d to %d\n",
                              j, ts->tasks[j].P, bi));
                *change = TRUE;
                ts->tasks[j].PT = bi;
            }
        }
    }
}

void respect_constraints_randomly (struct task_set* ts)
{
    int i, j, k, change;
    int cnt = 0;

    do {
        change = FALSE;

        for (i=0; i<ts->num_task_clusters; i++) {
            struct task_cluster* cluster = &ts->task_clusters[i];
            for (j=0; j<cluster->num_tasks; j++) {
                int tj = cluster->tasks[j];
                for (k=j+1; k<cluster->num_tasks; k++) {
                    int tk = cluster->tasks[k];
                    if (ts->tasks[tj].P < ts->tasks[tk].PT) {
                        change = TRUE;
                        if (rand_double() < 0.5) {
                            ts->tasks[tk].PT = ts->tasks[tj].P;
                        }
                        else {
                            // ts->tasks[tj].P = ts->tasks[tk].PT;
                            assign_pri (ts, tj, ts->tasks[tk].PT);
                        }
                    }
                    if (ts->tasks[tk].P < ts->tasks[tj].PT) {
                        change = TRUE;
                        if (rand_double() < 0.5) {
                            ts->tasks[tj].PT = ts->tasks[tk].P;
                        }
                        else {
                            // ts->tasks[tk].P = ts->tasks[tj].PT;
                            assign_pri (ts, tk, ts->tasks[tj].PT);
                        }
                    }
                }
            }
        }

        respect_task_barriers (ts, &change);

        /*
         * this only happens when this routine can't converge on a
         * priority/threshold assignment that respects the constraints,
         * and this should have been caught earlier
         */
        cnt++;
        if (cnt > 10000) {
            assert (0);
        }

    }
    while (change);

    assert (constraints_valid (ts));
}

void respect_constraints (struct task_set* ts)
{
    int i, j, k, change;
    int cnt = 0;

    do {
        change = FALSE;

        for (i=0; i<ts->num_task_clusters; i++) {
            struct task_cluster* cluster = &ts->task_clusters[i];
            for (j=0; j<cluster->num_tasks; j++) {
                int tj = cluster->tasks[j];
                for (k=j+1; k<cluster->num_tasks; k++) {
                    int tk = cluster->tasks[k];
                    /*
                     * policy decision: make the constraint hold by raising thresholds
                     * instead of lowering priorities
                     */
                    if (ts->tasks[tj].P < ts->tasks[tk].PT) {
                        ts->tasks[tk].PT = ts->tasks[tj].P;
                        change = TRUE;
                    }
                    if (ts->tasks[tk].P < ts->tasks[tj].PT) {
                        ts->tasks[tj].PT = ts->tasks[tk].P;
                        change = TRUE;
                    }
                }
            }
        }

        respect_task_barriers (ts, &change);

        /*
         * this only happens when this routine can't converge on a
         * priority/threshold assignment that respects the constraints,
         * and this should have been caught earlier
         */
        cnt++;
        if (cnt > 10000) {
            assert (0);
        }

    }
    while (change);

    assert (constraints_valid (ts));
}
#endif
/*
 * create a new task */
int new_task (struct task_set* ts,
              time_value C,
              time_value T,
              time_value t,
              time_value n,
              time_value D,
              time_value J,
              time_value B,
              const char* name)
{
    int i, num;

    // basic checking to help avoid weird errors later on
    assert (ts);
    assert (C >= 0);
    assert (T > 0);
    assert (t > 0);
    assert (n > 0);
    assert (D > 0);
    assert (J >= 0);
    assert (B >= 0);
    assert (name);

    num = ts->num_tasks;

    if (num == ts->max_tasks) {
        printf ("oops: too many tasks\n");
        assert (0);
    }

    for (i=0; i<num; i++) {
        if (strcmp (ts->tasks[i].name, name) == 0) {
            printf ("oops: duplicate task name %s\n", name);
            assert (0);
        }
    }

    ts->tasks[num].C = C;
    ts->tasks[num].T = T;
    ts->tasks[num].t = t;
    ts->tasks[num].n = n;
    ts->tasks[num].D = D;
    ts->tasks[num].J = J;
    ts->tasks[num].B = B;
    strncpy (ts->tasks[num].name, name, MAX_NAMELEN);
    ts->tasks[num].P = -1;
    ts->tasks[num].PT = -1;
    ts->tasks[num].S = -1;
    ts->tasks[num].R = -1;
    ts->tasks[num].num = num;
    ts->tasks[num].thread = -1;

    ts->num_tasks++;

    return num;
}

void set_preempt_thresh (struct task_set* ts, int i, int PT)
{
    assert (ts);
    assert (i >= 0 && i<ts->num_tasks);
    assert (PT >= 0);

    DBGPrint(5,("change %s PT from %d to %d. \n",
                ts->tasks[i].name,
                ts->tasks[i].PT,
                PT));

    ts->tasks[i].PT = PT;
}

int get_preempt_thresh (struct task_set* ts, int i)
{
    assert (ts);
    assert (i >= 0 && i<ts->num_tasks);

    return ts->tasks[i].PT;
}

extern void assign_random_preempt_thresh (struct task_set* ts)
{
    int i;

    for (i=0; i<ts->num_tasks; i++) {
        if (ts->tasks[i].P == 0) {
            ts->tasks[i].PT = 0;
        }
        else {
            ts->tasks[i].PT = rand_long()%ts->tasks[i].P;
        }
    }
}

void set_pri (struct task_set* ts, int i, int P)
{
    assert (ts);
    assert (i >= 0 && i<ts->num_tasks);
    assert (P >= 0);

    ts->tasks[i].P = P;
}

int get_pri (struct task_set* ts, int i)
{
    assert (ts);
    assert (i >= 0 && i<ts->num_tasks);

    return ts->tasks[i].P;
}

/*
 * create a new task that is not sporadically periodic
 */
int new_simple_task (struct task_set* ts,
                     time_value C,
                     time_value T,
                     time_value D,
                     time_value J,
                     time_value B,
                     const char* name)
{
    return new_task (ts, C, T, T, 1, D, J, B, name);
}

int new_simple_task_with_pri (struct task_set* ts,
                              time_value C,
                              time_value T,
                              time_value D,
                              time_value J,
                              time_value B,
                              int P,
                              int PT,
                              const char* name)
{
    int i;
    i = new_task (ts, C, T, T, 1, D, J, B, name);
    ts->tasks[i].P = P;
    ts->tasks[i].PT = PT;
    return i;
}
#ifdef USE_CLUSTER
void new_sem (struct task_set* ts,
              const char* sem_name)
{
    int i, num;

    assert (ts);
    assert (sem_name);

    num = ts->num_sems;

    if (num == ts->max_sems) {
        printf ("oops: too many semaphores\n");
        assert (0);
    }

    for (i=0; i<ts->num_sems; i++) {
        if (strcmp (sem_name, ts->sems[i].name) == 0) {
            printf ("oops: duplicate semaphore name %s\n", sem_name);
            assert (0);
        }
    }

    strncpy (ts->sems[num].name, sem_name, MAX_NAMELEN);
    ts->sems[num].ceiling = -1;

    ts->num_sems++;
}

void new_lock (struct task_set* ts,
               const char* sem_name,
               const char* task_name,
               time_value B)
{
    int i, num;

    assert (ts);
    assert (sem_name);
    assert (task_name);
    assert (B >= 0);

    num = ts->num_locks;

    if (num == ts->max_locks) {
        printf ("oops: too many locks\n");
        assert (0);
    }

    ts->locks[num].task = NULL;
    for (i=0; i<ts->num_tasks; i++) {
        if (strcmp (task_name, ts->tasks[i].name) == 0) {
            ts->locks[num].task = &(ts->tasks[i]);
        }
    }
    if (!ts->locks[num].task) {
        printf ("oops: task %s not found\n", task_name);
        assert (0);
    }

    ts->locks[num].sem = NULL;
    for (i=0; i<ts->num_sems; i++) {
        if (strcmp (sem_name, ts->sems[i].name) == 0) {
            ts->locks[num].sem = &(ts->sems[i]);
        }
    }
    if (!ts->locks[num].sem) {
        printf ("oops: semaphore %s not found\n", sem_name);
        assert (0);
    }

    ts->locks[num].lock_time = B;

    ts->num_locks++;
}

void implement_clusters_using_locks (struct task_set* ts)
{
    int i;

    for (i=0; i<ts->num_task_clusters; i++) {
        char name[512];
        int j;

        sprintf (name, "cluster%d\n", i);
        new_sem (ts, name);
        for (j=0; j<ts->task_clusters[i].num_tasks; j++) {
            int t = ts->task_clusters[i].tasks[j];
            new_lock (ts, name, ts->tasks[t].name, ts->tasks[t].C);
        }
    }
}
#endif
/*
 * assign rate or deadline monotonic priorities to tasks
 */
void set_priorities (struct task_set* ts,
                     enum sched_way way)
{
    int i, j, k, pri, mintask = 0;
    time_value min;
    int* scratch;

    DBGPrint (5, ("######### setting priorities with method %d ##########\n", way));

    assert (ts);

    switch (way) {
#ifdef USE_CLUSTER
        case BY_CLUSTER:
            pri = ts->num_tasks-1;
            sort_task_barriers (ts);
            for (k=-1; k<ts->num_task_barriers; k++) {
                int bl, bh;
                if (k == -1) {
                    bl = ts->num_tasks-1;
                }
                else {
                    bl = ts->task_barriers[k];
                }
                if (k == ts->num_task_barriers-1) {
                    bh = -1;
                }
                else {
                    bh = ts->task_barriers[k+1];
                }
                DBGPrint (5, ("barrier %d at %d:%d\n", k, bl, bh));
                for (i=0; i<ts->num_task_clusters; i++) {
                    int ext = ts->task_clusters[i].tasks[0];
                    DBGPrint (5, ("  cluster %d ", i));
                    if (ext <= bl && ext > bh) {
                        DBGPrint (5, ("selected\n"));
                        for (j=0; j<ts->task_clusters[i].num_tasks; j++) {
                            int t = ts->task_clusters[i].tasks[j];
                            DBGPrint (5, ("    task %d gets pri %d\n", t, pri));
                            ts->tasks[t].P = pri--;
                        }
                    }
                    else {
                        DBGPrint (5, ("skipped\n"));
                    }
                }
            }
            break;
#endif
        case INORDER:
            for (i=0; i<ts->num_tasks; i++) {
                ts->tasks[i].P = i;
            }
            make_all_preemptible (ts);
            break;

        case RM:
        case DM:
            scratch = (int*) xmalloc (sizeof (int) * ts->num_tasks);

            for (i=0; i<ts->num_tasks; i++) {
                scratch[i] = 0;
            }

            for (i=0; i<ts->num_tasks; i++) {
                min = MAX_TIME_VALUE;
                for (j=0; j<ts->num_tasks; j++) {
                    time_value selection;
                    if (way == RM) {
                        selection = ts->tasks[j].T;
                    }
                    else {
                        selection = ts->tasks[j].D;
                    }

                    if (selection < min && !scratch[j]) {
                        mintask = j;
                        min = selection;
                    }
                }
                assert (min != MAX_TIME_VALUE);
                ts->tasks[mintask].P = i;
                scratch[mintask] = 1;
            }

            for (i=0; i<ts->num_tasks; i++) {
                assert (scratch[i] != 0);
            }

            xfree (scratch);

            make_all_preemptible (ts);
            break;

        default:
            assert (0);
    }
}

static void calc_ceiling (struct task_set* ts,
                          struct sem* s)
{
    int i;

    // initialize to impossibly low priority
    s->ceiling = ts->num_tasks+1;

    for (i=0; i<ts->num_locks; i++) {
        if (ts->locks[i].sem == s) {
            s->ceiling = tvmin (s->ceiling, ts->locks[i].task->P);
        }
    }
}

static void max_block_time (struct task_set* ts,
                            int i)
{
    int j, max_lock;

    max_lock = 0;
    for (j=0; j<ts->num_locks; j++) {
        if (ts->locks[j].sem->ceiling <= ts->tasks[i].P &&
            ts->locks[j].task->P > ts->tasks[i].P) {
            max_lock = tvmax (max_lock, ts->locks[j].lock_time);
        }
    }

    ts->tasks[i].B = max_lock;
}

void calculate_blocking_pcp (struct task_set* ts)
{
    int i;

    assert (ts);

    for (i=0; i<ts->num_sems; i++) {
        calc_ceiling (ts, &ts->sems[i]);
    }

    for (i=0; i<ts->num_tasks; i++) {
        max_block_time (ts, i);
    }
}

/*
 * return single task utilization
 */
double utilization_task (struct task_set* ts, int i)
{
    assert (ts);
    return (double)ts->tasks[i].C / (double)ts->tasks[i].T;
}

/*
 * return task set utilization
 */
double utilization_set (struct task_set* ts)
{
    int i;
    double U;

    assert (ts);

    for (i=0, U=0; i<ts->num_tasks; i++) {
        U += utilization_task (ts, i);
    }

    return U;
}

void assign_pri (struct task_set* ts,
                 int t,
                 int new_pri)
{
    int old_pri;

    assert (ts);
    assert (t >= 0 && t < ts->num_tasks);
    assert (new_pri >= 0 && new_pri < ts->num_tasks);

    old_pri = ts->tasks[t].P;
    if (old_pri == new_pri) return;

    if (new_pri > old_pri) {
        int j;
        for (j=0; j<ts->num_tasks; j++) {
            int Pj = ts->tasks[j].P;
            if (Pj <= new_pri && Pj > old_pri) {
                ts->tasks[j].P--;
                if (ts->tasks[j].PT > 0) {
                    ts->tasks[j].PT--;
                }
            }
        }
    }
    else {
        int j;
        for (j=0; j<ts->num_tasks; j++) {
            int Pj = ts->tasks[j].P;
            if (Pj >= new_pri && Pj < old_pri) {
                ts->tasks[j].P++;
                if (ts->tasks[j].PT < ts->num_tasks-1) {
                    ts->tasks[j].PT++;
                }
            }
        }
    }
    ts->tasks[t].P = new_pri;
}

void assign_only_pri (struct task_set* ts,
                      int t,
                      int new_pri)
{
    int old_pri;

    assert (ts);
    assert (t >= 0 && t < ts->num_tasks);
    assert (new_pri >= 0 && new_pri < ts->num_tasks);

    old_pri = ts->tasks[t].P;
    if (old_pri == new_pri) return;

    if (new_pri > old_pri) {
        int j;
        for (j=0; j<ts->num_tasks; j++) {
            int Pj = ts->tasks[j].P;
            if (Pj <= new_pri && Pj > old_pri) {
                ts->tasks[j].P--;
            }
        }
    }
    else {
        int j;
        for (j=0; j<ts->num_tasks; j++) {
            int Pj = ts->tasks[j].P;
            if (Pj >= new_pri && Pj < old_pri) {
                ts->tasks[j].P++;
            }
        }
    }
    ts->tasks[t].P = new_pri;
}

/*
 * return number of tasks
 */
int num_tasks (struct task_set* ts)
{
    assert (ts);
    return ts->num_tasks;
}

/*
 * print task set in format usable by arbdead
 */
void dump_arbdead_file (struct task_set* ts)
{
    FILE* outf;
    int i;
    char* fn;

    assert (ts);

    fn = (char*) xmalloc (strlen (ts->name) + 5);
    strncpy (fn, ts->name, strlen (ts->name) + 1);
    strcat (fn, ".txt");

    outf = fopen (fn, "w");
    assert (outf);

    fprintf (outf, "parameters %d %d %d %d 9999999\n",
             ts->Tclk, ts->Cclk, ts->Cql, ts->Cqs);

    for (i=0; i<ts->num_tasks; i++) {
        fprintf (outf, "task %s %d %d %d %d %d %d %d\n",
                 ts->tasks[i].name,
                 ts->tasks[i].T,
                 ts->tasks[i].t,
                 ts->tasks[i].n,
                 ts->tasks[i].D,
                 ts->tasks[i].C,
                 ts->tasks[i].J,
                 ts->tasks[i].B);
    }

    xfree (fn);
    fclose (outf);
}

struct task_set* fload_task_set (FILE* fp)
{
    int res;
    struct task_set* ts = create_task_set (50, 50, 50, 50,
                                           "loaded_task_set",
                                           1000, 0, 0, 0,
                                           "Wang00_fixed");
    char line[4096];

    while (fgets (line, 4096, fp)) {
        time_value C, T, t = 0, n = 0, D, J, B = 0, P, PT = 0, S, R, Thr;
        double U;
        char name[MAX_NAMELEN];
        int task;

        res = 0;
        res = sscanf (line, "%s %d %d %d %d %d %d %d %lf %d %d %d\n",
                      name, &C, &T, &t, &n, &D, &J, &B, &U, &P, &S, &R);
        if (res != 12) {
            res = sscanf (line, "%s %d %d %d %d %lf %d %d %d %d %d\n",
                          name, &C, &T, &D, &J, &U, &P, &PT, &S, &R, &Thr);
            if (res != 11) continue;
            t = T;
            n = 1;
        }

        if (n == 0) n = 1000;

        task = new_task (ts, C, T, t, n, D, J, B, name);
        ts->tasks[task].P = P;
        ts->tasks[task].PT = PT;
    }

    return ts;
}

struct task_set* load_task_set (const char* fn)
{
    struct task_set* ts;
    FILE* inf;

    inf = fopen (fn, "r");
    if (!inf) return NULL;
    ts = fload_task_set (inf);
    fclose (inf);
    return ts;
}

/*
 * print task set in somewhat human-readable form
 */
void fprint_task_set (struct task_set* ts, FILE* fp)
{
    int i;

    assert (ts);

    fprintf (fp, "task set %s\n", ts->name);
    fprintf (fp, "parameters  Tclk %d  Cclk %d  Cql %d  Cqs %d\n",
             ts->Tclk, ts->Cclk, ts->Cql, ts->Cqs);

    if (is_ts_sporadically_periodic (ts)) {
        fprintf (fp, "Task Name       C       T       t       n       D       J       B       U       P       S       R\n");
        for (i=0; i<ts->num_tasks; i++) {
            fprintf (fp, "%9s%8d%8d%8d%8d%8d%8d%8d %0.5f%8d%8d%8d\n",
                     ts->tasks[i].name,
                     ts->tasks[i].C,
                     ts->tasks[i].T,
                     ts->tasks[i].t,
                     ts->tasks[i].n,
                     ts->tasks[i].D,
                     ts->tasks[i].J,
                     ts->tasks[i].B,
                     utilization_task (ts, i),
                     ts->tasks[i].P,
                     ts->tasks[i].S,
                     ts->tasks[i].R);
        }
    }
    else {
#ifdef USE_DVS
        fprintf (fp, "Task Name       C       T       D       R       U       P       PT      S       Cu      F\n");
        for (i=0; i<ts->num_tasks; i++) {
            fprintf (fp, "%9s%8d%8d%8d%8d %0.5f%8d%8d%8d%8d %0.5f\n",
                     ts->tasks[i].name,
                     ts->tasks[i].C,
                     ts->tasks[i].T,
                     ts->tasks[i].D,
                     ts->tasks[i].R,
                     utilization_task (ts, i),
                     ts->tasks[i].P,
                     ts->tasks[i].PT,
                     ts->tasks[i].S,
                     ts->tasks[i].Cu,
                     valid_f_scale[ts->tasks[i].f]);
        }
#else
        fprintf (fp, "Task Name       C       T       D       J       U       P       PT      S       R       Thr\n");
        for (i=0; i<ts->num_tasks; i++) {
            fprintf (fp, "%9s%8d%8d%8d%8d %0.5f%8d%8d%8d%8d%8d\n",
                     ts->tasks[i].name,
                     ts->tasks[i].C,
                     ts->tasks[i].T,
                     ts->tasks[i].D,
                     ts->tasks[i].J,
                     utilization_task (ts, i),
                     ts->tasks[i].P,
                     ts->tasks[i].PT,
                     ts->tasks[i].S,
                     ts->tasks[i].R,
                     ts->tasks[i].thread);
        }
#endif
    }

    if (ts->max_task_clusters) {
        fprintf (fp, "%d / %d task clusters\n",
                 ts->num_task_clusters,
                 ts->max_task_clusters);
        for (i=0; i<ts->num_task_clusters; i++) {
            int j;
            fprintf (fp, "task cluster %d: ", i);
            for (j=0; j<ts->task_clusters[i].num_tasks; j++) {
                fprintf (fp, "%d ", ts->task_clusters[i].tasks[j]);
            }
            fprintf (fp, "\n");
        }
    }

    if (ts->num_task_barriers) {
        fprintf (fp, "%d / %d task barriers\n",
                 ts->num_task_barriers, MAX_BARRIERS);
        if (ts->num_task_barriers > 0) {
            fprintf (fp, "barriers at: ");
            for (i=0; i<ts->num_task_barriers; i++) {
                fprintf (fp, "%d ", ts->task_barriers[i]);
            }
            fprintf (fp, "\n");
        }
    }
}

void print_task_set (struct task_set* ts)
{
    fprint_task_set (ts, stdout);
}

void normalize_utilization (struct task_set* ts, double norm)
{
    int i;
    double U;

    assert (ts);

    U = utilization_set (ts);

    for (i=0; i<ts->num_tasks; i++) {
        ts->tasks[i].C = (time_value) (ts->tasks[i].C / U);
    }
}

int wcetcmp (struct task_set* ts1, struct task_set* ts2)
{
    int i;

    assert (ts1);
    assert (ts2);

    if (ts1->num_tasks != ts2->num_tasks) return 1;

    for (i=0; i<ts1->num_tasks; i++) {
        if (ts1->tasks[i].C != ts2->tasks[i].C) return 1;
    }

    return 0;
}

int is_ts_sporadically_periodic (struct task_set* ts)
{
    int i;
    int ret = FALSE;

    assert (ts);

    for (i=0; i<ts->num_tasks; i++) {
        if (ts->tasks[i].n != 1 ||
            ts->tasks[i].t != ts->tasks[i].T) {
            ret = TRUE;
        }
    }

    return ret;
}

int has_overheads (struct task_set* ts)
{
    assert (ts);

    if (ts->Cql == 0 && ts->Cqs == 0 && ts->Cclk == 0) {
        return FALSE;
    }
    else {
        return TRUE;
    }
}

int ensure_same_response_times (struct task_set* ts1,
                                struct task_set* ts2)
{
    int i;

    assert (ts1->num_tasks == ts2->num_tasks);

    /*
     * make sure response times are the same, but only when the tasks
     * are schedulable
     */
    for (i=0; i<ts1->num_tasks; i++) {
        if (ts1->tasks[i].S != ts2->tasks[i].S) return FALSE;
        if (ts1->tasks[i].S && ts1->tasks[i].R != ts2->tasks[i].R) return FALSE;
    }

    return TRUE;
}

int second_worse(struct task_set* ts1,
                 struct task_set* ts2)
{
    int i;
    int howbad = 0;

    assert (ts1->num_tasks == ts2->num_tasks);

    for (i=0; i<ts1->num_tasks; i++) {
        if (ts2->tasks[i].R > ts1->tasks[i].R) {
            howbad += ts2->tasks[i].R - ts1->tasks[i].R;
        }
    }

    return howbad;
}

int no_zero_wcet (struct task_set* ts)
{
    int i;

    for (i=0; i<ts->num_tasks; i++) {
        assert (ts->tasks[i].C >= 0);
        if (ts->tasks[i].C == 0) {
            return FALSE;
        }
    }

    return TRUE;
}

void save_task_set_source_code (struct task_set* ts)
{
    int i;
    FILE* outf;
    char fn[MAX_NAMELEN+20];

    assert (ts);

    sprintf (fn, "task_sets/%s.c", ts->name);
    outf = fopen (fn, "w");
    assert (outf);

    // FIXME -- make sure this is a simple task set

    for (i=0; i<ts->num_tasks; i++) {
        fprintf (outf, "new_simple_task (ts, %d, %d, %d, %d, %d, \"%s\");\n",
                 ts->tasks[i].C, ts->tasks[i].T, ts->tasks[i].D,
                 ts->tasks[i].J, ts->tasks[i].B, ts->tasks[i].name);
    }

    fclose (outf);
}

void save_task_set_source_code_with_pri (struct task_set* ts)
{
    int i;
    FILE* outf;
    char fn[MAX_NAMELEN+20];

    assert (ts);

    sprintf (fn, "task_sets/%s.c", ts->name);
    outf = fopen (fn, "w");
    assert (outf);

    // FIXME -- make sure this is a simple task set

    for (i=0; i<ts->num_tasks; i++) {
        fprintf (outf, "new_simple_task_with_pri (ts, %d, %d, %d, %d, %d, %d, %d, \"%s\");\n",
                 ts->tasks[i].C,
                 ts->tasks[i].T,
                 ts->tasks[i].D,
                 ts->tasks[i].J,
                 ts->tasks[i].B,
                 ts->tasks[i].P,
                 ts->tasks[i].PT,
                 ts->tasks[i].name);
    }

    fclose (outf);
}

void save_task_set_latex (struct task_set* ts)
{
    int i;
    FILE* outf;
    char fn[MAX_NAMELEN+20];

    assert (ts);

    sprintf (fn, "task_sets/%s.tex", ts->name);
    outf = fopen (fn, "w");
    assert (outf);

    // FIXME -- make sure this is a simple task set

    fprintf (outf, "\\begin{tabular}{|l|r|r|r|r|r|r|}\n");
    fprintf (outf, "\\hline\n");
    fprintf (outf, "Name& C& T& D& J& B\\\\\n");
    fprintf (outf, "\\hline\n");

    for (i=0; i<ts->num_tasks; i++) {
        fprintf (outf, "%s& %d& %d& %d& %d& %d\\\\\n",
                 ts->tasks[i].name, ts->tasks[i].C, ts->tasks[i].T,
                 ts->tasks[i].D, ts->tasks[i].J, ts->tasks[i].B);
    }

    fprintf (outf, "\\hline\n");
    fprintf (outf, "\\end{tabular}\n");

    fclose (outf);
}

time_value find_max_deadline (struct task_set* ts)
{
    int i, max = 0;
    for (i=0; i<ts->num_tasks; i++) {
        if (ts->tasks[i].D > max) max = ts->tasks[i].D;
    }
    return max;
}
#ifdef USE_CLUSTER
int are_all_tasks_in_clusters (struct task_set* ts)
{
    int i;

    for (i=0; i<ts->num_tasks; i++) {
        if (!in_cluster (ts, i)) return FALSE;
    }

    return TRUE;
}

int requires_runtime_pt_support (struct task_set* ts)
{
    int i, j, x;

    if (!are_all_tasks_in_clusters (ts)) return TRUE;

#ifdef JOIN_CLUSTERS
    {
        int* joined = (int*) xmalloc (sizeof (int) * ts->num_task_clusters);
        int c;

        for (i=0; i<ts->num_task_clusters; i++) {
            joined[i] = -1;
        }
        c = 0;
        for (i=0; i<ts->num_task_clusters; i++) {
            c++;
            for (x=0; x<ts->num_task_clusters; x++) {
                for (j=0; j<ts->num_task_clusters; j++) {
                    if (are_clusters_joinable (ts, i, j) &&
                        ts->task_clusters[i].merge) {
                        joined[i] = c;
                    }
                }
            }
        }
        for (i=0; i<ts->num_task_clusters; i++) {
            int pt = -1;
            for (j=0; j<ts->task_clusters[i].num_tasks; j++) {
                int t;
                if (joined[i] != joined[j]) continue;
                t = ts->task_clusters[i].tasks[j];
                if (pt == -1) pt = ts->tasks[t].PT;
                if (pt != ts->tasks[t].PT) return TRUE;
            }
        }

        xfree (joined);
    }
#else

    for (i=0; i<ts->num_task_clusters; i++) {
        for (j=0; j<ts->num_task_clusters; j++) {
            int maxi, maxj, mini, minj;
            if (i == j) continue;
            maxi = maxp_pt (ts, i);
            maxj = maxp_pt (ts, j);
            mini = minp_pt (ts, i);
            minj = minp_pt (ts, j);
            if (!((maxi > minj) || (mini < maxj))) return TRUE;
        }
    }

#endif

    return FALSE;
}
#endif
#ifdef USE_DVS
time_value modify_task_C_by_freq(time_value Cu, freq_scale freq)
{
    double tmp = (double)Cu/freq;

    return (time_value)ceil(tmp);
}

time_value get_Cu(struct task_set* ts, int t)
{
    assert (ts);
    assert (t<num_tasks(ts));

    return ts->tasks[t].Cu;
}

char* get_task_name(struct task_set* ts, int t)
{
    assert (ts);
    assert (t<num_tasks(ts));

    return ts->tasks[t].name;
}

int new_dvs_task (struct task_set* ts,
                  time_value Cu,
                  time_value T,
                  time_value t,
                  time_value n,
                  time_value D,
                  time_value J,
                  time_value B,
                  freq_level f,
                  const char* name)
{
    int num;
    time_value C = 0;
    // basic checking to help avoid weird errors later on
    assert (Cu >= 0);
    assert (f >= 0);
    assert (f <= MAX_FREQ_LEVEL);

    C = modify_task_C_by_freq(Cu, valid_f_scale[f]);
    num = new_task(ts,C,T,t,n,D,J,B,name);
    ts->tasks[num].Cu = Cu;
    ts->tasks[num].f = f;

    return num;
}

int new_simple_dvs_task(struct task_set* ts,
                        time_value Cu,
                        time_value T,
                        time_value D,
                        time_value J,
                        time_value B,
                        freq_level f,
                        const char* name)
{
    return new_dvs_task (ts, Cu, T, T, 1, D, J, B, f, name);
}

int new_simple_dvs_task_with_pri(struct task_set* ts,
                                 time_value Cu,
                                 time_value T,
                                 time_value D,
                                 time_value J,
                                 time_value B,
                                 freq_level f,
                                 int P,
                                 int PT,
                                 const char* name)
{
    int i;
    i = new_dvs_task (ts, Cu, T, T, 1, D, J, B, f, name);
    ts->tasks[i].P = P;
    ts->tasks[i].PT = PT;
    return i;
}

#endif


