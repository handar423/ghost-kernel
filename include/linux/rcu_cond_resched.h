#ifndef __LINUX_RCUCONDRESCHED_H
#define __LINUX_RCUCONDRESCHED_H

#include <linux/percpu.h>
#include <linux/rcupdate.h>

/*
 * Hooks for cond_resched() and friends to avoid RCU CPU stall warnings.
 */

#define RCU_COND_RESCHED_LIM 256	/* ms vs. 100s of ms. */
DECLARE_PER_CPU(int, rcu_cond_resched_count);
void rcu_resched(void);

/*
 * Is it time to report RCU quiescent states?
 *
 * Note unsynchronized access to rcu_cond_resched_count.  Yes, we might
 * increment some random CPU's count, and possibly also load the result from
 * yet another CPU's count.  We might even clobber some other CPU's attempt
 * to zero its counter.  This is all OK because the goal is not precision,
 * but rather reasonable amortization of rcu_note_context_switch() overhead
 * and extremely high probability of avoiding RCU CPU stall warnings.
 * Note that this function has to be preempted in just the wrong place,
 * many thousands of times in a row, for anything bad to happen.
 */
static inline bool rcu_should_resched(void)
{
	return __this_cpu_inc_return(rcu_cond_resched_count) >=
	       RCU_COND_RESCHED_LIM;
}

/*
 * Report quiscent states to RCU if it is time to do so.
 */
static inline void rcu_cond_resched(void)
{
	if (unlikely(rcu_should_resched()))
		rcu_resched();
}

#endif /* __LINUX_RCUCONDRESCHED_H */
