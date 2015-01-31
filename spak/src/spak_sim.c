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

double OVERRUN_FRAC;

static time_value sim_time;
static int sim_finished;
static struct task* current;
static struct task_set* sim_ts;
static FILE* outfile;
static time_value last_reschedule;
static time_value last_record;
static int total_misses, total_hits;

#ifdef USE_DVS
static energy_value energy_sum;
static power_value  power;
#define idle_f_level   (MIN_FREQ_LEVEL)
#endif
#ifdef USE_COUNT_DISPATCH
static int dispatch_count = 0;
#endif

enum event_type {
    ARRIVE = 8122,
    EXPIRATION,
    RELEASE,
    DEADLINE
};

struct event {
    enum event_type type;
    struct task* task;
    struct task_instance* inst;
};

struct task_instance {
    time_value arrival;
    int deferred;
    int completed;
    int missed;
    struct task_instance* next;
};

static struct event* new_event (enum event_type type)
{
    struct event* e = (struct event*) xmalloc (sizeof (struct event));
    e->type = type;
    return e;
}

static void insert_event (struct event* e, time_value t)
{
    pri_q_insert (t, (void*)e);
}

static void record_runtime (struct task* t)
{
    freq_scale freq = 0;
    if (last_record != sim_time) {
        const char* c = (t) ? t->name : "idle";

#ifdef USE_DVS
        freq = (t)? valid_f_scale[t->f]: valid_f_scale[idle_f_level];
        energy_sum += 1.0*(sim_time-last_record)*pow(freq, 3.0);
#endif
        DBGPrint (5, ("%d -- %d : %s\n",
                      last_record, sim_time, c));
        if (outfile) fprintf (outfile, "run %s %d %d\n", c, last_record, sim_time);
    }

    last_record = sim_time;
}

/*
 * make next_task start running
 */
static void dispatch (struct task* next_task)
{
    struct event* e;

    if (current) {
        current->state = READY;
    }

    DBGPrint (5, ("current was %s (effP = %d), is now %s (effP = %d)\n",
                  (current) ? current->name : "none",
                  (current) ? current->effP : -1,
                  next_task->name,
                  next_task->effP));
#ifdef USE_COUNT_DISPATCH
    dispatch_count += (current) ? 1 : 0,
#endif
                      record_runtime (current);
    current = next_task;
    current->state = RUNNING;
    current->last_scheduled = sim_time;

    e = new_event (EXPIRATION);
    insert_event (e, sim_time + current->budget);
}

static struct task_instance* new_task_instance (void)
{
    struct task_instance* ti;

    ti = (struct task_instance*) xmalloc (sizeof (struct task_instance));
    ti->arrival = sim_time;
    ti->completed = FALSE;
    ti->missed = FALSE;
    ti->deferred = FALSE;
    ti->next = NULL;

    return ti;
}

static void run_instance (struct task* t, struct task_instance* ti)
{
    t->cur_inst = ti;
    // t->budget = t->C + (time_value)(OVERRUN_FRAC * t->C * rand_double());
    t->budget = t->C + (time_value)(OVERRUN_FRAC * t->C);
    t->state = READY;
    if (outfile) fprintf (outfile, "release %s %d\n", t->name, sim_time);
}

static struct task_instance* get_deferred_instance (struct task* t)
{
    struct task_instance* ti = t->next_inst;

    if (!ti) return NULL;

    t->next_inst = t->next_inst->next;
    if (!t->next_inst) {
        t->last_inst = NULL;
    }

    assert (ti->deferred);
    ti->deferred = FALSE;

    return ti;
}

static void do_accounting (void)
{
    int deduction;

    if (!current) return;

    assert (current->state == RUNNING);
    assert (current->cur_inst);

    deduction = sim_time - last_reschedule;
    current->budget -= deduction;
    assert (current->budget >= 0);

    /*
     * lazily set effective priority to be preemption threshold
     */
    if (deduction > 0) {
        if (current->effP != current->PT) {
            DBGPrint (5, ("lazily boosting priority of %s from %d to %d\n",
                          current->name, current->effP, current->PT));
        }
        current->effP = current->PT;
    }

    if (current->budget == 0) {
        int response_time;
        struct task_instance* ti;

        response_time = sim_time - current->cur_inst->arrival;

        if (current->cur_inst->missed) {
            // deadline has already been missed -- we clean up this instance
            DBGPrint (5, ("time %d : %s expiring; DEADLINE MISSED; response time was %d\n",
                          sim_time, current->name, response_time));
            if (outfile) fprintf (outfile, "missed %s %d\n", current->name, sim_time);
            xfree (current->cur_inst);
        }
        else {
            // deadline not missed -- the deadline event handler will clean up the instance
            DBGPrint (5, ("time %d : %s expiring; response time was %d\n",
                          sim_time, current->name, response_time));
            if (outfile) fprintf (outfile, "completed %s %d\n", current->name, sim_time);
            current->cur_inst->completed = TRUE;
        }
        current->cur_inst = NULL;

        // at expiration, effective priority drops to normal
        current->effP = current->P;

#if 1
        if (response_time >= current->max_response_time) {
            if (response_time == current->max_response_time) {
                current->max_rt_seen++;
            }
            else {
                current->max_response_time = response_time;
                current->timeof_max_response_time = sim_time;
                current->max_rt_seen = 1;
            }
        }
#else
        if (response_time > current->R && current->max_response_time == 0) {
            current->max_response_time = response_time;
            current->timeof_max_response_time = sim_time;
        }
#endif

        ti = get_deferred_instance(current);
        if (ti) {
            struct event* e;
            run_instance (current, ti);
            e = new_event (EXPIRATION);
            insert_event (e, sim_time);
        }
        else {
            current->state = EXPIRED;
        }
        record_runtime (current);
        current = NULL;
    }
}

/*
 * run the scheduler
 */
static void reschedule (void)
{
    struct task* candidate;

    do_accounting ();

    /*
     * find the highest-effective-priority ready task, if any
     */
    candidate = NULL;
    {
        int i;
        for (i=0; i<sim_ts->num_tasks; i++) {

            if (sim_ts->tasks[i].state == READY) {
                DBGPrint (6, ("  %s has effective priority %d\n",
                              sim_ts->tasks[i].name, sim_ts->tasks[i].effP));
            }

            if (sim_ts->tasks[i].state == READY) {

                if (!candidate) {
                    candidate = &sim_ts->tasks[i];
                    continue;
                }

                if (sim_ts->tasks[i].effP < candidate->effP) {
                    candidate = &sim_ts->tasks[i];
                    continue;
                }

                /*
                 * give preference to boosted tasks -- weird, but necessary to
                 * make preemption thresholds work
                 */
                if (sim_ts->tasks[i].effP == candidate->effP) {
                    /*
                     * the two tasks have the same priority; only take the new
                     * candidate if it is boosted and the current one is not --
                     * ouch!
                     */
                    if (sim_ts->tasks[i].effP != sim_ts->tasks[i].P &&
                        candidate->effP == candidate->P) {
                        candidate = &sim_ts->tasks[i];
                        continue;
                    }
                }

            }
        }
    }

    DBGPrint (5, ("reschedule: candidate is %s\n",
                  (candidate) ? candidate->name : "NONE"));

    if (candidate && (!current || candidate->effP < current->effP)) {
        DBGPrint (5, ("reschedule: about to dispatch %s\n",
                      candidate->name));
        dispatch (candidate);
    }
    else {
        DBGPrint (5, ("reschedule: not dispatching any task\n"));
    }

    last_reschedule = sim_time;
}

static void arrive (struct task* t)
{
    struct task_instance* ti;
    ti = new_task_instance ();

    // if (outfile) fprintf (outfile, "arrive %s %d\n", t->name, sim_time);

    reschedule ();

    // schedule check for missed deadline
    {
        struct event* e = new_event (DEADLINE);
        e->task = t;
        e->inst = ti;
        insert_event (e, sim_time + t->D);
    }

    // schedule subsequent arrival of this task
    {
        struct event* e = new_event (ARRIVE);
        int add;
        e->task = t;

#if 1
        // randomly mess with task phasing
        if (rand_double() < t->phase_prob) {
            if (rand_double() < 0.2) {
                add = rand_long() % t->T;
            }
            else {
                add = (rand_long() % t->T) / 5;
            }
        }
        else {
            add = 0;
        }
#else
        add = 0;
#endif

        insert_event (e, sim_time + t->T + add);
    }

    // schedule release of this task
    {
        struct event* e = new_event (RELEASE);
        time_value te;
        double r;

        e->task = t;
        e->inst = ti;
        r = rand_double();
        if (r < 0.33) {
            te = sim_time;
        }
        else if (r < 0.66) {
            te = sim_time + t->J;
        }
        else {
            int j = rand_long() % (t->J+1);
            assert (j >= 0 && j <= t->J);
            te = sim_time + j;
        }

        /*
         * enforce the constraint that this task instance can't arrive
         * before the previous one
         */
        if (te < t->last_arrival) {
            te = t->last_arrival;
        }
        t->last_arrival = te+1;

        insert_event (e, te);
        DBGPrint (5, ("         task %s will be released at time %d\n",
                      e->task->name, te));
    }
}

static void deadline (struct task* t, struct task_instance* ti)
{
    assert (t);
    assert (ti);

    if (outfile) fprintf (outfile, "deadline %s %d\n", t->name, sim_time);

    if (ti->completed) {
        xfree (ti);
        total_hits++;
    }
    else {
        ti->missed = TRUE;
        total_misses++;
    }
}

static void defer_instance (struct task* t, struct task_instance* ti)
{
    assert (!ti->deferred);
    ti->deferred = TRUE;
    if (t->next_inst) {
        assert (t->last_inst);
        t->last_inst->next = ti;
        t->last_inst = ti;
    }
    else {
        assert (!t->last_inst);
        t->next_inst = t->last_inst = ti;
    }
}

static void release (struct task* t, struct task_instance* ti)
{
    assert (t);
    assert (ti);

    if (t->cur_inst) {
        defer_instance (t, ti);
    }
    else {
        run_instance (t, ti);
        reschedule ();
    }
}

static void process_event (struct event* e, time_value now)
{
    // something very wrong if this is not true
    assert (now >= sim_time);

    sim_time = now;

    switch (e->type) {
        case ARRIVE:
            DBGPrint (5, ("time %d: processing ARRIVE event for task %s\n",
                          sim_time, e->task->name));
            arrive (e->task);
            break;

        case EXPIRATION:
            DBGPrint (5, ("time %d: processing EXPIRATION event\n", sim_time));
            reschedule ();
            break;

        case RELEASE:
            DBGPrint (5, ("time %d: processing RELEASE event for task %s\n",
                          sim_time, e->task->name));
            release (e->task, e->inst);
            break;

        case DEADLINE:
            DBGPrint (5, ("time %d: processing DEADLINE event for task %s\n",
                          sim_time, e->task->name));
            deadline (e->task, e->inst);
            break;

        default:
            assert (0);
    }
}

void simulate (struct task_set* taskset,
               time_value end_time,
               const char* outfile_name,
               double overrun_frac,
               const char* overrun_str,
               FILE* miss_file)
{
    int i;
    int all_schedulable;

    OVERRUN_FRAC = overrun_frac;

    DBGPrint (5, ("starting simulation\n"));

    init_pri_q ();
    current = NULL;
    assert (taskset);
    assert (!sim_ts);
    sim_ts = taskset;

    sim_time = 0;
    last_reschedule = 0;
    last_record = 0;
    sim_finished = FALSE;
    total_misses = 0;
    total_hits = 0;

    if (outfile_name) {
        outfile = fopen (outfile_name, "w");
        if (!outfile) {
            printf ("oops: couldn't open output file\n");
        }
    }
    else {
        outfile = NULL;
    }

    for (i=0; i<sim_ts->num_tasks; i++) {
        struct event* e;

        assert (sim_ts->tasks[i].P >= 0 && sim_ts->tasks[i].P < sim_ts->num_tasks);
        assert (sim_ts->tasks[i].PT >= 0 && sim_ts->tasks[i].PT < sim_ts->num_tasks);

        sim_ts->tasks[i].budget = 0;
        sim_ts->tasks[i].max_response_time = 0;
        sim_ts->tasks[i].max_rt_seen = 0;
        sim_ts->tasks[i].timeof_max_response_time = -1;
        sim_ts->tasks[i].state = EXPIRED;
        sim_ts->tasks[i].cur_inst = NULL;
        sim_ts->tasks[i].next_inst = NULL;
        sim_ts->tasks[i].last_inst = NULL;
        sim_ts->tasks[i].effP = sim_ts->tasks[i].P;
        sim_ts->tasks[i].last_arrival = 0;

#define PHASE_TIMES 10

        // probability of changing phase, per invocation
        sim_ts->tasks[i].phase_prob = (1.0 * sim_ts->tasks[i].T / end_time) * PHASE_TIMES;

        /*
        printf ("task %d period %d phase prob %f\n",
            i, sim_ts->tasks[i].T, sim_ts->tasks[i].phase_prob);
        */

        e = new_event (ARRIVE);
        e->task = &sim_ts->tasks[i];
        insert_event (e, 0);

        if (outfile) fprintf (outfile, "pri %s %d\n",
                                  sim_ts->tasks[i].name, sim_ts->tasks[i].P);
    }

    if (outfile) fprintf (outfile, "pri idle %d\n", sim_ts->num_tasks);

    while (!sim_finished) {
        struct event* e;
        time_value now;
        now = pri_q_extract_min ((void**)&e);
        assert (e);
        process_event (e, now);
        xfree (e);
        if (sim_time >= end_time) sim_finished = TRUE;
    }

    /*
     * avoid leaking memory by draining the event queue
     *
     * cleaning up task instances is slightly tricky; the relevant
     * invariant is that every task instance is (1) pointed to by an
     * upcoming deadline event and (2) is either a task's cur_inst or on
     * its deferred list
     */
    while (1) {
        struct event* e;
        pri_q_extract_min ((void**)&e);
        if (!e) break;
        if (e->type == DEADLINE) {
            if (!e->inst->deferred) {
                for (i=0; i<sim_ts->num_tasks; i++) {
                    if (sim_ts->tasks[i].cur_inst == e->inst) goto out;
                }
                xfree (e->inst);
out:
                ;
            }
        }
        xfree (e);
    }

    DBGPrint (5, ("simulation finished\n"));

    all_schedulable = TRUE;
    for (i=0; i<sim_ts->num_tasks; i++) {
        if (sim_ts->tasks[i].S != TRUE) all_schedulable = FALSE;
    }

    if (outfile) {
        fclose (outfile);
    }

    DBGPrint (3, ("simulation results:\n"));

    assert (sim_ts->Analysis.valid (sim_ts));
    feasible (sim_ts, TRUE);

    for (i=0; i<sim_ts->num_tasks; i++) {

        // clean up remaining task instances
        while (1) {
            struct task_instance* ti = get_deferred_instance (&sim_ts->tasks[i]);
            if (!ti) break;
            xfree (ti);
        }
        if (sim_ts->tasks[i].cur_inst) {
            xfree (sim_ts->tasks[i].cur_inst);
        }

        DBGPrint (3, ("  %s max resp time: analytic %d, sim %d (at %d) (off by %d) (%d times)\n",
                      sim_ts->tasks[i].name,
                      sim_ts->tasks[i].R,
                      sim_ts->tasks[i].max_response_time,
                      sim_ts->tasks[i].timeof_max_response_time,
                      (sim_ts->tasks[i].R - sim_ts->tasks[i].max_response_time),
                      sim_ts->tasks[i].max_rt_seen));
        if (all_schedulable && OVERRUN_FRAC == 0.0) {
            if (sim_ts->tasks[i].max_response_time > sim_ts->tasks[i].R) {
                save_task_set_source_code_with_pri (sim_ts);
                print_task_set (sim_ts);
                assert (0);
            }
        }
    }

    if (OVERRUN_FRAC != 0.0) {
        // printf ("%f %d\n", overrun_frac, total_misses);
        fprintf (miss_file, "%d %d\n", (int)(100*overrun_frac), total_misses);
    }

    deinit_pri_q ();
    sim_ts = NULL;
}
#ifdef USE_DVS
void simulate_power (struct task_set* taskset,
                     time_value end_time,
                     FILE* power_fp)
{
    energy_sum = 0;
    power = 0;
#ifdef USE_COUNT_DISPATCH
    dispatch_count = 0;
#endif
    simulate (taskset,end_time,NULL,0.0,NULL,NULL);

    fprintf(power_fp, "time = %d\t energy = %f\t dispatch = %d\n",
            end_time,
            energy_sum,
            dispatch_count);
}
#endif
