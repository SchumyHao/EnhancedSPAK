/*
 * SPAK internal header file --- not for client use
 */
 #ifndef __SPAK_TASKS_H__
 #define __SPAK_TASKS_H__

#include <math.h>

#define JOIN_CLUSTERS 1

enum task_state {
  RUNNING = 2323, 
  READY,
  BLOCKED,
  EXPIRED
};

struct task_instance;

#define MAX_TASKS_PER_CLUSTER 50

#define MAX_BARRIERS 12

struct task_cluster {
  int num_tasks;
  int tasks[MAX_TASKS_PER_CLUSTER];
  char name[MAX_NAMELEN];
  int merge;
};

#define MAX_PREEMPT_CONSTRAINTS 50

struct task {
  /*
   * for analysis
   */
  time_value C;    // WCET
  time_value T;    // outer period or minimum interarrival time
  time_value t;    // innter period or minimum interarrival time
  time_value n;    // burst size
  time_value D;    // deadline
  time_value J;    // jitter
  time_value B;    // blocking
  time_value R;    // response time
  int P;           // priority
  int PT;          // preemption threshold
  int S;           // schedulable?
  char name[MAX_NAMELEN];
  int num;
#ifdef USE_DVS
  time_value Cu;   // WCET in MAX frequency
  freq_level f;
#endif

  /*
   * for simulation
   */
  time_value last_scheduled;
  time_value budget;
  time_value max_response_time;
  time_value timeof_max_response_time;
  int max_rt_seen;
  enum task_state state;
  struct task_instance *cur_inst, *next_inst, *last_inst;
  int effP; // current effective priority (for preemption threshold scheduling)
  int last_arrival;
  double phase_prob;

  /*
   * for preemption threshold analysis
   */
  time_value tag;
  int thread;      // implementation thread
};

struct sem {
  char name[MAX_NAMELEN];
  int ceiling;
};

struct lock {
  struct task *task;
  struct sem *sem;
  time_value lock_time;
};

// interface to analysis routines

struct spak_analysis {
  int (*valid)(struct task_set *);
  time_value (*response_time)(struct task_set *, int, time_value);
};

extern int get_analysis1_ptrs (const char *id, struct spak_analysis *A);
extern int get_analysis2_ptrs (const char *id, struct spak_analysis *A);
extern int get_analysis3_ptrs (const char *id, struct spak_analysis *A);
extern int get_analysis4_ptrs (const char *id, struct spak_analysis *A);
extern int get_analysis5_ptrs (const char *id, struct spak_analysis *A);
extern int get_analysis6_ptrs (const char *id, struct spak_analysis *A);
#ifdef USE_DVS
extern int get_analysis7_ptrs (const char *id, struct spak_analysis *A);
#endif

struct task_set {
  int num_tasks;
  int max_tasks;
  struct task *tasks;

  int num_sems;
  int max_sems;
  struct sem *sems;

  int num_locks;
  int max_locks;
  struct lock *locks;

  int num_task_clusters;
  int max_task_clusters;
  struct task_cluster *task_clusters;

  int task_barriers[MAX_BARRIERS];
  int num_task_barriers;

  char name[MAX_NAMELEN];

  time_value Cql, Cqs, Cclk, Tclk;

  struct spak_analysis Analysis;
};

extern int ANNEAL_MAX;
extern double INIT_TEMP;
extern double TEMP_SCALE;

extern int is_ts_sporadically_periodic (struct task_set *ts);
extern int has_overheads (struct task_set *ts);

extern void assign_pri (struct task_set *ts,
			int task,
			int new_pri);

extern void assign_only_pri (struct task_set *ts,
			     int task,
			     int new_pri);

extern void set_preempt_thresh_equal_to_pri (struct task_set *ts);

extern int internal_set_analysis (struct spak_analysis *A, const char *which);

extern int is_thresh_lower_than_pri (struct task_set *ts);

extern void permute_pri_and_thresh (struct task_set *ts);

extern void permute_pri (struct task_set *ts);

extern int no_zero_wcet (struct task_set *ts);

extern int uses_preempt_thresh_analysis (struct task_set *ts);

extern void print_cluster (struct task_set *ts, int c);

extern void permute_npt (struct task_set *ts);

extern void set_preemption_thresholds_npt (struct task_set *ts);

extern void sort_task_barriers (struct task_set *ts);

extern int minpri (struct task_set *ts, int c);

extern int maxpri (struct task_set *ts, int c);

#ifdef JOIN_CLUSTERS
extern int are_clusters_joinable (struct task_set *ts, int i, int j);
#endif

extern int is_pri_unique (struct task_set *ts);

extern int has_jitter (struct task_set *ts);

#endif
