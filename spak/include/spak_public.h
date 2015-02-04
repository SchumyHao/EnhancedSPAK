/*
 * SPAK external header file --- to be included by clients
 */
#ifndef __SPAK_PUBLIC_H__
#define __SPAK_PUBLIC_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include <time.h>

extern int ANNEAL_MAX;
extern double INIT_TEMP;
extern double TEMP_SCALE;

#define DBGPrint(lev,str) do {      \
        if ((lev)<=DBG_LEVEL) printf str; \
        fflush (stdout);                  \
    } while (0);

// just a few things needed to compile using VC++
#ifdef WIN32
#pragma warning( disable : 4514 4127 4100 4505 )
#define inline __inline
static inline void seed_rand (int s)
{
    srand(s);
}
static inline long int rand_long (void)
{
    return rand();
}
static inline double rand_double (void)
{
    return ((double)rand()) / RAND_MAX;
}
#else
#include <unistd.h>
static inline double rand_double (void)
{
    return drand48();
}
static inline long int rand_long (void)
{
    return lrand48();
}
static inline void seed_rand (int s)
{
    srand48(s);
}
#endif

#define TRUE (1)
#define FALSE (0)

#define MILLION (1000 * 1000)
#define BILLION (1000 * MILLION)

// #define LONG_LONG 1

#ifdef LONG_LONG
typedef long long int time_value;
#define MAX_TIME_VALUE (LONG_LONG_MAX)
#else
typedef int time_value;
#define MAX_TIME_VALUE (INT_MAX)
#endif


#ifdef USE_DVS
typedef double freq_scale;
static freq_scale valid_f_scale[]= {0.1,0.3,0.5,0.7,0.9,1.0};
typedef int freq_level;
#define SIZE_OF_ARRAY(a)   ((sizeof(a))/(sizeof(a[0])))
#define MAX_FREQ_LEVEL       (SIZE_OF_ARRAY(valid_f_scale)-1)
#define MIN_FREQ_LEVEL       (0)
typedef double power_value;
typedef double energy_value;
#endif

struct task_set;

extern struct task_set* copy_task_set (struct task_set* ts1);

extern struct task_set* create_task_set (int max_tasks,
        int max_sems,
        int max_locks,
        int max_clusters,
        const char* name,
        time_value Tclk,
        time_value Cclk,
        time_value Cql,
        time_value Cqs,
        const char* which);

extern int new_task (struct task_set* ts,
                     time_value C,
                     time_value T,
                     time_value t,
                     time_value n,
                     time_value D,
                     time_value J,
                     time_value B,
                     const char* Name);

extern int new_simple_task (struct task_set* ts,
                            time_value C,
                            time_value T,
                            time_value D,
                            time_value J,
                            time_value B,
                            const char* Name);

extern int new_simple_task_with_pri (struct task_set* ts,
                                     time_value C,
                                     time_value T,
                                     time_value D,
                                     time_value J,
                                     time_value B,
                                     int P,
                                     int PT,
                                     const char* name);

extern void new_sem (struct task_set* ts,
                     const char* sem_name);

extern void new_lock (struct task_set* ts,
                      const char* sem_name,
                      const char* task_name,
                      time_value B);

enum sched_way {
    RM = 1000,
    DM,
    INORDER,
    BY_CLUSTER,
};

extern void set_priorities (struct task_set* ts,
                            enum sched_way way);

extern int assign_optimal_pri (struct task_set* ts);

extern void calculate_blocking_pcp (struct task_set* ts);

extern double utilization_set (struct task_set* ts);

extern double utilization_task (struct task_set* ts, int i);

extern int feasible (struct task_set* ts, int all);

extern int feasible_one_task (struct task_set* ts, int i);

extern int num_tasks (struct task_set* ts);

extern void dump_arbdead_file (struct task_set* ts);

extern void print_task_set (struct task_set* ts);

extern void fprint_task_set (struct task_set* ts, FILE* fp);

extern struct task_set* fload_task_set (FILE* fp);

extern struct task_set* load_task_set (const char* fn);

extern double find_critical_scale (struct task_set* ts,
                                   struct task_set** ts2);

extern time_value max_resp;

extern struct task_set* maximize_insensitivity_by_annealing (struct task_set* ts,
        int PT,
        int runtime_pt);

extern struct task_set* exhaustive (struct task_set* ts);

extern struct task_set* greedy (struct task_set* ts);

extern void free_task_set (struct task_set* ts);

extern void normalize_utilization (struct task_set* ts, double norm);

extern void simulate (struct task_set* taskset,
                      time_value end_time,
                      const char* outfile_name,
                      double overrun_frac,
                      const char* overrun_str,
                      FILE* miss_file);

extern int wcetcmp (struct task_set* ts1, struct task_set* ts2);

static inline time_value tvmax (time_value tv1,
                                time_value tv2)
{
    if (tv1 > tv2) {
        return tv1;
    }
    else {
        return tv2;
    }
}

static inline time_value tvmin (time_value tv1,
                                time_value tv2)
{
    if (tv1 < tv2) {
        return tv1;
    }
    else {
        return tv2;
    }
}

extern void make_all_preemptible (struct task_set* ts);

extern void make_all_nonpreemptible (struct task_set* ts);

extern int is_all_preemptible (struct task_set* ts);

extern int is_all_nonpreemptible (struct task_set* ts);

#if 0
static inline double diff_time (struct timeval* a,
                                struct timeval* b)
{
    return
        ((double)1e6*a->tv_sec + a->tv_usec) -
        ((double)1e6*b->tv_sec + b->tv_usec);
}
#endif

extern void print_xmalloc_cnt (void);

extern void set_analysis (struct task_set* ts, const char* which);

extern void set_preempt_thresh (struct task_set* ts, int i, int PT);

extern int get_preempt_thresh (struct task_set* ts, int i);

extern void set_pri (struct task_set* ts, int i, int P);

extern int get_pri (struct task_set* ts, int i);

extern int assign_optimal_preemption_thresholds (struct task_set* ts);

extern int ensure_same_response_times (struct task_set* ts1,
                                       struct task_set* ts2);

extern int second_worse (struct task_set* ts1,
                         struct task_set* ts2);

extern void randomize_priorities (struct task_set* ts);

extern int greedy_priorities_and_thresholds (struct task_set* ts,
        int pt_support);

extern int exhaustive_assign_optimal_priorities_and_thresholds (struct task_set* ts);

extern int optimal_partition_into_threads (struct task_set* ts);

extern void maximize_preempt_thresholds (struct task_set* ts);

extern void minimize_threads_by_annealing (struct task_set* ts1,
        int test_overrun,
        int pt_support);

extern int anneal_priorities_and_thresholds (struct task_set** ts,
        int pt_support);

extern void save_task_set_source_code (struct task_set* ts);

extern void save_task_set_source_code_with_pri (struct task_set* ts);

extern void save_task_set_latex (struct task_set* ts);

struct task_cluster;

extern struct task_cluster* new_task_cluster (struct task_set* ts,
        const char* name);

extern void add_to_task_cluster (struct task_set* ts,
                                 struct task_cluster* cluster,
                                 const char* task);

extern int has_task_barriers (struct task_set* ts);

extern int has_task_clusters (struct task_set* ts);

extern int has_constraints (struct task_set* ts);

extern int constraints_valid (struct task_set* ts);

extern void respect_constraints (struct task_set* ts);

extern void respect_constraints_randomly (struct task_set* ts);

extern time_value find_max_deadline (struct task_set* ts);

extern void new_task_barrier (struct task_set* ts, int x);

extern int in_cluster (struct task_set* ts, int i);

extern int requires_runtime_pt_support (struct task_set* ts);

extern int are_all_tasks_in_clusters (struct task_set* ts);

extern void random_preemp_thresh_tests (void);

extern int preemption_thresh_test (struct task_set* ts, int num);

extern void create_clusters_for_singletons (struct task_set* ts);

extern struct task_set* create_random_task_set (int num,
        int total_tasks,
        int max_deadline,
        const char* analysis,
        int multiplier,
        int has_jitter,
        int has_independent_deadline);
extern struct task_set* create_random_task_set_with_utilization (int num,
        int total_tasks,
        int max_deadline,
        const char* analysis,
        int multiplier,
        double set_u,
        int has_jitter,
        int has_independent_deadline);

extern int compare_preempt_thresh_analyses (struct task_set* ts1);

extern void break_orig_preempt_thresh_analysis (void);

extern void ensure_target_doesnt_need_pt (struct task_set* ts);

extern void put_all_tasks_in_one_cluster (struct task_set* ts);

extern void set_jitter (struct task_set* ts, int t, int J);

extern void change_wcet (struct task_set* ts, int t, int inc);

extern void look_for_p_schedule (struct task_set* ts,
                                 struct task_set** new_ts);

extern time_value get_period (struct task_set* ts, int t);

extern void create_random_task_cluster (struct task_set* ts, int num);

extern int barriers_permit_pri (struct task_set* ts, int t, int pri);

extern void create_random_task_barrier (struct task_set* ts);

extern void implement_clusters_using_locks (struct task_set* ts);

extern void assign_random_preempt_thresh (struct task_set* ts);

extern struct task_set* maximize_insensitivity_optimal (struct task_set* ts);

extern void avionics_test (void);

extern void find_hg_task_set (void);

extern void set_wcet (struct task_set* ts, int t, time_value newC);

extern time_value get_wcet (struct task_set* ts, int t);

extern void spak_srand (void);

#ifdef USE_DVS
extern time_value modify_task_C_by_freq(time_value Cu, freq_scale freq);

extern time_value get_Cu(struct task_set* ts, int t);

extern char* get_task_name(struct task_set* ts, int t);

extern int new_dvs_task (struct task_set* ts,
                         time_value Cu,
                         time_value T,
                         time_value t,
                         time_value n,
                         time_value D,
                         time_value J,
                         time_value B,
                         freq_level f,
                         const char* name);

extern int new_simple_dvs_task(struct task_set* ts,
                               time_value Cu,
                               time_value T,
                               time_value D,
                               time_value J,
                               time_value B,
                               freq_level f,
                               const char* name);

extern int new_simple_dvs_task_with_pri(struct task_set* ts,
                                        time_value Cu,
                                        time_value T,
                                        time_value D,
                                        time_value J,
                                        time_value B,
                                        freq_level f,
                                        int P,
                                        int PT,
                                        const char* name);

extern freq_level get_task_frequency_level(struct task_set* ts, int t);

extern void set_task_frequency_level(struct task_set* ts, int t, freq_level f);

extern void inc_task_frequency_level(struct task_set* ts, int t);

extern void dec_task_frequency_level(struct task_set* ts, int t);

extern power_value calculate_tast_set_average_power(struct task_set* ts);

extern freq_level get_task_set_frequency_level(struct task_set* ts);

extern void set_task_set_frequency_level(struct task_set* ts, freq_level f);

extern void inc_task_set_frequency_level(struct task_set* ts);

extern void dec_task_set_frequency_level(struct task_set* ts);

void simulate_power (struct task_set* taskset,
                     time_value end_time,
                     FILE* power_fp);
#endif

#endif
