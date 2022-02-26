#ifndef _LINUX_BH_H
#define _LINUX_BH_H

#include <linux/preempt.h>
#include <linux/preempt_mask.h>

#ifdef CONFIG_PREEMPT_RT_FULL

extern void __local_bh_disable(void);
extern void _local_bh_enable(void);
extern void __local_bh_enable(void);

static inline void local_bh_disable(void)
{
	__local_bh_disable();
}

static inline void __local_bh_disable_ip(unsigned long ip, unsigned int cnt)
{
	__local_bh_disable();
}

static inline void local_bh_enable(void)
{
	__local_bh_enable();
}

static inline void __local_bh_enable_ip(unsigned long ip, unsigned int cnt)
{
	__local_bh_enable();
}

static inline void local_bh_enable_ip(unsigned long ip)
{
	__local_bh_enable();
}

#else /* PREEMPT_RT_FULL */

#ifdef CONFIG_TRACE_IRQFLAGS
extern void __local_bh_disable_ip(unsigned long ip, unsigned int cnt);
#else
static __always_inline void __local_bh_disable_ip(unsigned long ip, unsigned int cnt)
{
	add_preempt_count(cnt);
	barrier();
}
#endif

extern void local_bh_disable(void);
extern void _local_bh_enable(void);
extern void local_bh_enable(void);
extern void local_bh_enable_ip(unsigned long ip);
extern void __local_bh_enable_ip(unsigned long ip, unsigned int cnt);

#endif /* PREEMPT_RT_FULL */

#endif /* _LINUX_BH_H */
