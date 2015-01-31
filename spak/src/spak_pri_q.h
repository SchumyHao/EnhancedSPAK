/*
 * SPAK internal header file --- not for client use
 */
#ifndef __SPAK_PRI_Q_H__
#define __SPAK_PRI_Q_H__
extern void init_pri_q (void);
extern void deinit_pri_q (void);
extern time_value pri_q_extract_min (void **addr);
extern void pri_q_insert (time_value key, void *ptr);
#endif
