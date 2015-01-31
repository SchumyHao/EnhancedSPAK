/*
 *  FP_PTDVS.c
 *
 *  Created on: Jan 9, 2015
 *      Author: schumy
 */

#include <math.h>
#include "spak_public.h"

typedef struct {
    energy_value v;
    struct task_set* ts;
} tab_item_t;

#define TOTAL_TASKS_NUMBER   (10)

FILE* log_fp = NULL;
FILE* power_fp = NULL;

static struct task_set* create_radom_task_set_by_utilization(const double u)
{
    struct task_set* ts = NULL;
    spak_srand();
    do {
        if(NULL!=ts) {
            free_task_set(ts);
            ts = NULL;
        }
        ts = create_random_task_set_with_utilization (1,  //number
                TOTAL_TASKS_NUMBER,      //Total tasks
                1000,                    //max_deadline
                "ee_fppt",               //analysis
                1,                       //multiplier
                u,                       //set_u
                FALSE,                   //has_jitter
                FALSE);                  //has_independent_deadline
        set_priorities (ts, INORDER);
    }
    while(feasible(ts, TRUE)!=TOTAL_TASKS_NUMBER);

    return ts;
}

static struct task_set* create_special_task_set(void)
{
	struct task_set* ts = NULL;
    ts = create_task_set (10, 25, 25, 10,
                          "special task set",
                          10000, 0, 0, 0,
                          "ee_fppt");
    new_dvs_task (ts,22,988,988,1,988,0,0,MAX_FREQ_LEVEL,"t0");
    new_dvs_task (ts,32,253,253,1,253,0,0,MAX_FREQ_LEVEL,"t1");
    new_dvs_task (ts,14,169,169,1,169,0,0,MAX_FREQ_LEVEL,"t2");
    new_dvs_task (ts,14,382,382,1,382,0,0,MAX_FREQ_LEVEL,"t3");
    new_dvs_task (ts,21,620,620,1,620,0,0,MAX_FREQ_LEVEL,"t4");
    new_dvs_task (ts,77,805,805,1,805,0,0,MAX_FREQ_LEVEL,"t5");
    new_dvs_task (ts,35,472,472,1,472,0,0,MAX_FREQ_LEVEL,"t6");
    new_dvs_task (ts,14,276,276,1,276,0,0,MAX_FREQ_LEVEL,"t7");
    new_dvs_task (ts,86,781,781,1,781,0,0,MAX_FREQ_LEVEL,"t8");
    new_dvs_task (ts,84,839,839,1,839,0,0,MAX_FREQ_LEVEL,"t9");
    set_priorities (ts, INORDER);
    feasible(ts, TRUE);
	return ts;
}

static int set_taskset_MPTA(struct task_set* ts)
{
    if(!assign_optimal_preemption_thresholds(ts)) {
        return FALSE;
    }
    maximize_preempt_thresholds(ts);
    return TRUE;
}

static int set_taskset_IPTA(struct task_set* ts)
{
    if(!assign_optimal_preemption_thresholds(ts)) {
        return FALSE;
    }
    return TRUE;
}


static void set_lowest_freq_level_for_all_tasks(struct task_set* ts)
{
    struct task_set* ts_copy = copy_task_set(ts);
    freq_level old_f_level = MAX_FREQ_LEVEL;

    while(MIN_FREQ_LEVEL < (old_f_level=get_task_set_frequency_level(ts_copy))) {
        dec_task_set_frequency_level(ts_copy);
        if(!set_taskset_MPTA(ts_copy)) {
            break;
        }
    }
    free_task_set(ts_copy);
    set_task_set_frequency_level(ts, old_f_level);
    set_taskset_MPTA(ts);
}

static int find_task_by_pri(struct task_set* ts, const int p)
{
    int i = 0;
    while(p!=get_pri(ts, i++))
        ;
    return --i;
}

static time_value sum_cu_in_same_freq_level(struct task_set* ts, const freq_level f)
{
    int i = 0;
    time_value cu_sum = 0;

    for(i=num_tasks(ts)-1; i>=0; i--) {
        cu_sum += (f == get_task_frequency_level(ts,i))? get_Cu(ts,i): 0;
    }

    return cu_sum;
}

static int try_feasible_with_MPTA(struct task_set* ts)
{
    int ret = FALSE;
    struct task_set* ts_copy = copy_task_set(ts);

    if(set_taskset_MPTA(ts_copy)) {
        ret = TRUE;
    }
    free_task_set(ts_copy);

    return ret;
}

static int try_feasible_after_task_slowdown(struct task_set* ts, const int t)
{
    int ret = FALSE;
    struct task_set* ts_copy = copy_task_set(ts);
    freq_level f = get_task_frequency_level(ts_copy, t);
    assert (f>MIN_FREQ_LEVEL);

    set_task_frequency_level(ts_copy, t, f-1);
    if(try_feasible_with_MPTA(ts_copy)) {
        ret = TRUE;

    }
    free_task_set(ts_copy);

    return ret;
}

static inline void* xmalloc (size_t size)
{
    void* p = malloc (size);
    if (!p) {
        printf ("oops: out of memory\n");
        assert (0);
    }
    return p;
}

static inline time_value get_task_C_at_set_f(struct task_set* ts, const int t, const freq_level f)
{
    assert (f >= MIN_FREQ_LEVEL);
    assert (f <= MAX_FREQ_LEVEL);

    return modify_task_C_by_freq(get_Cu(ts,t),valid_f_scale[f]);
}

static time_value get_C_after_task_slowdown(struct task_set* ts, const int t)
{
    freq_level f = get_task_frequency_level(ts, t);
    assert (f > MIN_FREQ_LEVEL);

    return get_task_C_at_set_f(ts, t, f-1);
}

static inline energy_value get_task_energy_at_set_f(struct task_set* ts, const int t, const freq_level f)
{
    assert (f >= MIN_FREQ_LEVEL);
    assert (f <= MAX_FREQ_LEVEL);

    return 1.0*get_task_C_at_set_f(ts,t,f)*pow(valid_f_scale[f], 3.0);
}

static inline energy_value calculate_saved_energy_after_task_slowdown(struct task_set* ts, const int t)
{
    freq_level f = get_task_frequency_level(ts, t);
    assert (f > MIN_FREQ_LEVEL);

    return (get_task_energy_at_set_f(ts,t,f) - get_task_energy_at_set_f(ts,t,f-1));
}

static int is_accept_tab_item(tab_item_t* tab, tab_item_t* old_tab, const int t)
{
    if(try_feasible_after_task_slowdown(old_tab->ts, t)) {
        energy_value task_saved_energy = calculate_saved_energy_after_task_slowdown(old_tab->ts,t);
        if(tab->v < (old_tab->v+task_saved_energy)) {
            return TRUE;
        }
    }
    return FALSE;
}

static void do_accept_tab_item(tab_item_t* tab, tab_item_t* old_tab, const int t)
{
    tab->v = old_tab->v + calculate_saved_energy_after_task_slowdown(old_tab->ts,t);
    free_task_set(tab->ts);
    tab->ts = copy_task_set(old_tab->ts);
    dec_task_frequency_level(tab->ts, t);
    set_taskset_MPTA(tab->ts);
}

static struct task_set* find_max_low_energy_task_set_from_tab(void* p, int r, int c)
{
    tab_item_t (*tab)[c+1] = (tab_item_t (*)[c+1])p;
    energy_value max_value = 0;
    int max_r = 0;
    int max_c = 0;
    int i,j;

    for(i=1; i<=r; i++) {
        for(j=1; j<=c; j++) {
            if(max_value < tab[i][j].v) {
                max_value = tab[i][j].v;
                max_r = i;
                max_c = j;
            }
        }
    }

    return tab[max_r][max_c].ts;
}

static void free_tab(void* p, int r, int c)
{
    tab_item_t (*tab)[c+1] = (tab_item_t (*)[c+1])p;
    int i,j;

    for (j=0; j<=r; j++) {
        for(i=0; i<=c; i++) {
            free_task_set(tab[j][i].ts);
        }
    }
    free(tab);
}

static struct task_set* get_lowest_energy_taskset_on_sub_freq(struct task_set* ts, const freq_level f)
{
//#define PRINT_TABLE
    int p = 0;
    int item = 0;
    int time = 0;
    int task = 0;
    int i = 0;
    const int total_tasks_num = num_tasks(ts);
    const time_value max_time = modify_task_C_by_freq(sum_cu_in_same_freq_level(ts, f), valid_f_scale[f-1]);

    tab_item_t (*tab)[max_time+1] = (tab_item_t (*)[max_time+1])xmalloc(
                                        (total_tasks_num+1)*(max_time+1)*sizeof(tab_item_t));
    memset(tab,0,(total_tasks_num+1)*(max_time+1)*sizeof(tab_item_t));
    for(i=0; i<=max_time; i++) {
        tab[0][i].ts = copy_task_set(ts);
    }

#ifdef PRINT_TABLE
    fprintf(log_fp,"find max length of frequency=%f.\n",valid_f_scale[f-1]);
    fprintf(log_fp,"i\\t      ");
    for(i=0; i<=max_time; i++) {
        fprintf(log_fp,"%4d  ",i);
    }
    fprintf(log_fp,"\n");
#endif

    for(p=num_tasks(ts)-1,item=0; p>=0; p--) {
        task = find_task_by_pri(ts, p);
        if((f==get_task_frequency_level(ts,task)) &&
           try_feasible_after_task_slowdown(ts,task)) {
            item++;
            time_value d = get_C_after_task_slowdown(ts,task);
            for(i=0; i<=max_time; i++) {
                tab[item][i].v = tab[item-1][i].v;
                tab[item][i].ts = copy_task_set(tab[item-1][i].ts);
            }
            for(time=d; time<=max_time; time++) {
                if(is_accept_tab_item(&tab[item][time], &tab[item-1][time-d], task)) {
                    do_accept_tab_item(&tab[item][time], &tab[item-1][time-d], task);
                }
            }
#ifdef PRINT_TABLE
            fprintf(log_fp,"%s(p=%d) ",get_task_name(ts,task),get_pri(ts,task));
            for(i=0; i<=max_time; i++) {
                fprintf(log_fp,"%.3f ",tab[item][i].v);
            }
            fprintf(log_fp,"\n");
#endif
        }
    }

    struct task_set* ts_lowest = find_max_low_energy_task_set_from_tab(tab, item, max_time);
    free_task_set(ts);
    ts = copy_task_set(ts_lowest);

    free_tab(tab, item, max_time);

    return ts;
}

static int get_tasks_num_by_freq(struct task_set* ts, freq_level f)
{
    int count = 0;
    int i = 0;

    if((f>=MIN_FREQ_LEVEL)&&(f<=MAX_FREQ_LEVEL)) {
        for(i=num_tasks(ts)-1; i>=0; i--) {
            count += (f==get_task_frequency_level(ts,i))? 1: 0;
        }
    }

    return count;
}

static freq_level get_taskset_lowest_frequency_level(struct task_set* ts)
{
    freq_level f = MAX_FREQ_LEVEL;
    int i = 0;

    for(i=num_tasks(ts)-1; i>=0; i--) {
        f = (f>get_task_frequency_level(ts,i))? get_task_frequency_level(ts,i): f;
    }

    return f;
}

static struct task_set* set_lowest_freq_level_for_each_task(struct task_set* ts)
{
    freq_level f = get_task_set_frequency_level(ts);
    struct task_set* ts_lowest = copy_task_set(ts);
    free_task_set(ts);

    do {
        f = get_taskset_lowest_frequency_level(ts_lowest);
        if(f>MIN_FREQ_LEVEL) {
            ts_lowest = get_lowest_energy_taskset_on_sub_freq(ts_lowest,f);
            //fprint_task_set (ts_lowest, log_fp);
        }
    }
    while(get_tasks_num_by_freq(ts_lowest,--f));

    return ts_lowest;
}

static void do_fp_ptdvs(struct task_set* ts_old)
{
    static int count = 0;
    struct task_set* ts = copy_task_set(ts_old);

    fprintf(power_fp, "usage = %f\t ",utilization_set(ts_old));
    fprintf(log_fp,"FP_PTDVS do %d times.\n",++count);
    fprintf(log_fp,"Origin Taskset.\n");
    fprint_task_set (ts, log_fp);
    set_taskset_MPTA(ts);

    set_lowest_freq_level_for_all_tasks(ts);

    ts = set_lowest_freq_level_for_each_task(ts);

    fprintf(log_fp,"Final Taskset.\n");
    assert(feasible(ts, TRUE)==num_tasks(ts));
    fprint_task_set (ts, log_fp);

    simulate_power(ts, 500000, power_fp);
    free_task_set(ts);
}

static int try_to_make_schedulable_ee_fppt(struct task_set* ts, int t)
{
	int ret = TRUE;
	int i,j,k = 0;
	struct task_set* ts_copy = copy_task_set(ts);

    for(j=num_tasks(ts_copy)-1; j>get_pri(ts_copy,t); j--) {
        int feas;
        int PT;
        while(((feas=feasible_one_task(ts_copy,j))==FALSE) &&
              ((PT=get_preempt_thresh(ts_copy,j))>0)) {
            set_preempt_thresh(ts_copy,j,--PT);
        }
        if(feas==FALSE) {
        	ret =  FALSE;
        	goto out;
        }
    }

    for(k=get_pri(ts_copy,t); k>=0; k--) {
        if(feasible_one_task(ts_copy,k)==FALSE) {
        	ret =  FALSE;
        	goto out;
        }
    }

    feasible(ts, TRUE);
    print_task_set (ts);
    feasible(ts_copy, TRUE);
    print_task_set (ts_copy);

    for(i=num_tasks(ts_copy)-1; i>=0; i--){
    	set_preempt_thresh(ts,i,get_preempt_thresh(ts_copy,i));
    }
    assert(feasible(ts, TRUE)==num_tasks(ts));
out:
	free_task_set(ts_copy);
    return ret;
}

static void do_ee_fppt(struct task_set* ts_old)
{
    static int count = 0;
    struct task_set* ts = copy_task_set(ts_old);
    int i = 0;
    freq_level old_f_level = MIN_FREQ_LEVEL;

    fprintf(power_fp, "usage = %f\t ",utilization_set(ts_old));
    fprintf(log_fp,"EE_FPPT do %d times.\n",++count);
    fprintf(log_fp,"Origin Taskset.\n");
    fprint_task_set (ts, log_fp);

    set_taskset_IPTA(ts);

    for(i=num_tasks(ts)-1; i>=0; i--) {
        while((feasible(ts, TRUE)==num_tasks(ts))&&
              (MIN_FREQ_LEVEL!=get_task_frequency_level(ts,i))) {
            old_f_level = get_task_frequency_level(ts,i);
            dec_task_frequency_level(ts,i);
        }
        if(feasible(ts, TRUE)!=num_tasks(ts)) {
            break;
        }
    }

    if(i>=0) {
    	if(!try_to_make_schedulable_ee_fppt(ts, i)){
    		set_task_frequency_level(ts,i,old_f_level);
    	}
    }

    fprintf(log_fp,"Final Taskset.\n");
    assert(feasible(ts, TRUE)==num_tasks(ts));
    fprint_task_set (ts, log_fp);

    simulate_power(ts, 500000, power_fp);
    free_task_set(ts);
}

static int get_task_by_cu_sort(struct task_set* ts, int n)
{
    assert(n>=0);
    assert(n<num_tasks(ts));

    int order[TOTAL_TASKS_NUMBER]= {0};
    int ptr = 0;
    int i,j = 0;

    for(i=0; i<TOTAL_TASKS_NUMBER; i++) {
        order[i] = TOTAL_TASKS_NUMBER;
    }

    for(i=0; i<TOTAL_TASKS_NUMBER; i++) {
        time_value cu = get_Cu(ts, i);
        for(ptr=0; ptr<TOTAL_TASKS_NUMBER; ptr++) {
            if(TOTAL_TASKS_NUMBER==order[ptr]) {
                order[ptr] = i;
                break;
            }
            else {
                if(cu > get_Cu(ts,order[ptr])) {
                    for(j=TOTAL_TASKS_NUMBER-1; j>ptr; j--) {
                        order[j] = order[j-1];
                    }
                    order[ptr] = i;
                    break;
                }
            }
        }
    }
    return order[n];
}

static void do_greedy(struct task_set* ts_old)
{
    static int count = 0;
    struct task_set* ts = copy_task_set(ts_old);
    int i = 0;
    freq_level old_f_level = MAX_FREQ_LEVEL;

    fprintf(power_fp, "usage = %f\t ",utilization_set(ts_old));
    fprintf(log_fp,"GREEDY do %d times.\n",++count);
    fprintf(log_fp,"Origin Taskset.\n");
    fprint_task_set (ts, log_fp);
    //print_task_set (ts);

    for(i=0; i<num_tasks(ts); i++) {
        int task = get_task_by_cu_sort(ts,i);
        while((feasible(ts, TRUE)==num_tasks(ts))&&
              (MIN_FREQ_LEVEL!=get_task_frequency_level(ts,task))) {
            old_f_level = get_task_frequency_level(ts,task);
            dec_task_frequency_level(ts,task);
        }
        if(feasible(ts, TRUE)!=num_tasks(ts)) {
            set_task_frequency_level(ts, task, old_f_level);
            break;
        }
    }

    fprintf(log_fp,"Final Taskset.\n");
    assert(feasible(ts, TRUE)==num_tasks(ts));
    fprint_task_set (ts, log_fp);

    simulate_power(ts, 500000, power_fp);
    free_task_set(ts);
}

int main(int argc, char* argv[])
{
    max_resp = 100000;     //max response time
    ANNEAL_MAX = 10000;
    INIT_TEMP = 0.03;
    TEMP_SCALE = 0.99;
    double u = 0;
    static struct task_set* ts = NULL;
    int i = 0;

    FILE* fp_ptdvs_log_fp = fopen("fp_ptdvs.log","w");
    FILE* ee_fppt_log_fp = fopen("ee_fppt.log","w");
    FILE* greedy_log_fp = fopen("greedy.log","w");
    FILE* fp_ptdvs_power_fp = fopen("fp_ptdvs.power","w");
    FILE* ee_fppt_power_fp = fopen("ee_fppt.power","w");
    FILE* greedy_power_fp = fopen("greedy.power","w");

    for(u=0.05; u<2; u+=0.05) {
        for(i=0; i<10; i++) {
            ts = create_radom_task_set_by_utilization(u);
        	//ts = create_special_task_set();

            log_fp = fp_ptdvs_log_fp;
            power_fp = fp_ptdvs_power_fp;
            do_fp_ptdvs(ts);

            log_fp = ee_fppt_log_fp;
            power_fp = ee_fppt_power_fp;
            do_ee_fppt(ts);

            log_fp = greedy_log_fp;
            power_fp = greedy_power_fp;
            do_greedy(ts);

            free_task_set(ts);
        }
    }
    fclose(fp_ptdvs_log_fp);
    fclose(ee_fppt_log_fp);
    fclose(greedy_log_fp);
    fclose(fp_ptdvs_power_fp);
    fclose(ee_fppt_power_fp);
    fclose(greedy_power_fp);

    return 0;
}

