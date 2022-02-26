/*
 * Read-Copy Update definitions shared among RCU implementations.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright IBM Corporation, 2011
 *
 * Author: Paul E. McKenney <paulmck@linux.vnet.ibm.com>
 */

#ifndef __LINUX_RCU_H
#define __LINUX_RCU_H

#ifdef CONFIG_RCU_TRACE
#define RCU_TRACE(stmt) stmt
#else /* #ifdef CONFIG_RCU_TRACE */
#define RCU_TRACE(stmt)
#endif /* #else #ifdef CONFIG_RCU_TRACE */

/*
 * Process-level increment to ->dynticks_nesting field.  This allows for
 * architectures that use half-interrupts and half-exceptions from
 * process context.
 *
 * DYNTICK_TASK_NEST_MASK defines a field of width DYNTICK_TASK_NEST_WIDTH
 * that counts the number of process-based reasons why RCU cannot
 * consider the corresponding CPU to be idle, and DYNTICK_TASK_NEST_VALUE
 * is the value used to increment or decrement this field.
 *
 * The rest of the bits could in principle be used to count interrupts,
 * but this would mean that a negative-one value in the interrupt
 * field could incorrectly zero out the DYNTICK_TASK_NEST_MASK field.
 * We therefore provide a two-bit guard field defined by DYNTICK_TASK_MASK
 * that is set to DYNTICK_TASK_FLAG upon initial exit from idle.
 * The DYNTICK_TASK_EXIT_IDLE value is thus the combined value used upon
 * initial exit from idle.
 */
#define DYNTICK_TASK_NEST_WIDTH 7
#define DYNTICK_TASK_NEST_VALUE ((LLONG_MAX >> DYNTICK_TASK_NEST_WIDTH) + 1)
#define DYNTICK_TASK_NEST_MASK  (LLONG_MAX - DYNTICK_TASK_NEST_VALUE + 1)
#define DYNTICK_TASK_FLAG	   ((DYNTICK_TASK_NEST_VALUE / 8) * 2)
#define DYNTICK_TASK_MASK	   ((DYNTICK_TASK_NEST_VALUE / 8) * 3)
#define DYNTICK_TASK_EXIT_IDLE	   (DYNTICK_TASK_NEST_VALUE + \
				    DYNTICK_TASK_FLAG)

/*
 * debug_rcu_head_queue()/debug_rcu_head_unqueue() are used internally
 * by call_rcu() and rcu callback execution, and are therefore not part of the
 * RCU API. Leaving in rcupdate.h because they are used by all RCU flavors.
 */

#ifdef CONFIG_DEBUG_OBJECTS_RCU_HEAD
# define STATE_RCU_HEAD_READY	0
# define STATE_RCU_HEAD_QUEUED	1

extern struct debug_obj_descr rcuhead_debug_descr;

static inline void debug_rcu_head_queue(struct rcu_head *head)
{
	debug_object_activate(head, &rcuhead_debug_descr);
	debug_object_active_state(head, &rcuhead_debug_descr,
				  STATE_RCU_HEAD_READY,
				  STATE_RCU_HEAD_QUEUED);
}

static inline void debug_rcu_head_unqueue(struct rcu_head *head)
{
	debug_object_active_state(head, &rcuhead_debug_descr,
				  STATE_RCU_HEAD_QUEUED,
				  STATE_RCU_HEAD_READY);
	debug_object_deactivate(head, &rcuhead_debug_descr);
}
#else	/* !CONFIG_DEBUG_OBJECTS_RCU_HEAD */
static inline void debug_rcu_head_queue(struct rcu_head *head)
{
}

static inline void debug_rcu_head_unqueue(struct rcu_head *head)
{
}
#endif	/* #else !CONFIG_DEBUG_OBJECTS_RCU_HEAD */

extern void kfree(const void *);

static inline bool __rcu_reclaim(const char *rn, struct rcu_head *head)
{
	unsigned long offset = (unsigned long)head->func;

	if (__is_kfree_rcu_offset(offset)) {
		RCU_TRACE(trace_rcu_invoke_kfree_callback(rn, head, offset));
		kfree((void *)head - offset);
		return 1;
	} else {
		RCU_TRACE(trace_rcu_invoke_callback(rn, head));
		head->func(head);
		return 0;
	}
}

extern int rcu_expedited;

#ifdef CONFIG_RCU_STALL_COMMON

extern int rcu_cpu_stall_suppress;
int rcu_jiffies_till_stall_check(void);

#define rcu_ftrace_dump_stall_suppress() \
do { \
	if (!rcu_cpu_stall_suppress) \
		rcu_cpu_stall_suppress = 3; \
} while (0)

#define rcu_ftrace_dump_stall_unsuppress() \
do { \
	if (rcu_cpu_stall_suppress == 3) \
		rcu_cpu_stall_suppress = 0; \
} while (0)

#else /* #endif #ifdef CONFIG_RCU_STALL_COMMON */
#define rcu_ftrace_dump_stall_suppress()
#define rcu_ftrace_dump_stall_unsuppress()
#endif /* #ifdef CONFIG_RCU_STALL_COMMON */

/*
 * Dump the ftrace buffer, but only one time per callsite per boot.
 */
#define rcu_ftrace_dump(oops_dump_mode) \
do { \
	static atomic_t ___rfd_beenhere = ATOMIC_INIT(0); \
	\
	if (!atomic_read(&___rfd_beenhere) && \
	    !atomic_xchg(&___rfd_beenhere, 1)) { \
		tracing_off(); \
		rcu_ftrace_dump_stall_suppress(); \
		ftrace_dump(oops_dump_mode); \
		rcu_ftrace_dump_stall_unsuppress(); \
	} \
} while (0)

void rcu_early_boot_tests(void);

enum rcutorture_type {
       RCU_FLAVOR,
       RCU_BH_FLAVOR,
       RCU_SCHED_FLAVOR,
       SRCU_FLAVOR,
       INVALID_RCU_FLAVOR
};

#if defined(CONFIG_TREE_RCU) || defined(CONFIG_TREE_PREEMPT_RCU)
void rcutorture_get_gp_data(enum rcutorture_type test_type, int *flags,
			    unsigned long *gpnum, unsigned long *completed);
void rcutorture_record_test_transition(void);
void rcutorture_record_progress(unsigned long vernum);
void do_trace_rcu_torture_read(const char *rcutorturename,
			       struct rcu_head *rhp,
			       unsigned long secs,
			       unsigned long c_old,
			       unsigned long c);
#else
static inline void rcutorture_get_gp_data(enum rcutorture_type test_type,
					  int *flags,
					  unsigned long *gpnum,
					  unsigned long *completed)
{
	*flags = 0;
	*gpnum = 0;
	*completed = 0;
}
static inline void rcutorture_record_test_transition(void)
{
}
static inline void rcutorture_record_progress(unsigned long vernum)
{
}
#ifdef CONFIG_RCU_TRACE
void do_trace_rcu_torture_read(const char *rcutorturename,
			       struct rcu_head *rhp,
			       unsigned long secs,
			       unsigned long c_old,
			       unsigned long c);
#else
#define do_trace_rcu_torture_read(rcutorturename, rhp, secs, c_old, c) \
	do { } while (0)
#endif
#endif

#endif /* __LINUX_RCU_H */
