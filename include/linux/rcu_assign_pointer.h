#ifndef __LINUX_RCU_ASSIGN_POINTER_H__
#define __LINUX_RCU_ASSIGN_POINTER_H__
#include <linux/compiler.h>
#include <asm/barrier.h>

#define __rcu_assign_pointer(p, v, space) \
	do { \
		smp_wmb(); \
		(p) = (typeof(*v) __force space *)(v); \
	} while (0)

/**
 * rcu_assign_pointer() - assign to RCU-protected pointer
 * @p: pointer to assign to
 * @v: value to assign (publish)
 *
 * Assigns the specified value to the specified RCU-protected
 * pointer, ensuring that any concurrent RCU readers will see
 * any prior initialization.
 *
 * Inserts memory barriers on architectures that require them
 * (which is most of them), and also prevents the compiler from
 * reordering the code that initializes the structure after the pointer
 * assignment.  More importantly, this call documents which pointers
 * will be dereferenced by RCU read-side code.
 *
 * In some special cases, you may use RCU_INIT_POINTER() instead
 * of rcu_assign_pointer().  RCU_INIT_POINTER() is a bit faster due
 * to the fact that it does not constrain either the CPU or the compiler.
 * That said, using RCU_INIT_POINTER() when you should have used
 * rcu_assign_pointer() is a very bad thing that results in
 * impossible-to-diagnose memory corruption.  So please be careful.
 * See the RCU_INIT_POINTER() comment header for details.
 */
#define rcu_assign_pointer(p, v) \
	__rcu_assign_pointer((p), (v), __rcu)

#endif
