/*
 * Copyright 2021 Google LLC
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "sched.h"
#include <linux/filter.h>

#ifdef CONFIG_SCHED_CLASS_GHOST

/**
 * rcu_replace_pointer() - replace an RCU pointer, returning its old value
 * @rcu_ptr: RCU pointer, whose old value is returned
 * @ptr: regular pointer
 * @c: the lockdep conditions under which the dereference will take place
 *
 * Perform a replacement, where @rcu_ptr is an RCU-annotated
 * pointer and @c is the lockdep argument that is passed to the
 * rcu_dereference_protected() call used to read that pointer.  The old
 * value of @rcu_ptr is returned, and @rcu_ptr is set to @ptr.
 */
#define rcu_replace_pointer(rcu_ptr, ptr, c)				\
({									\
	typeof(ptr) __tmp = rcu_dereference_protected((rcu_ptr), (c));	\
	rcu_assign_pointer((rcu_ptr), (ptr));				\
	__tmp;								\
})

BPF_CALL_2(bpf_ghost_wake_agent, struct bpf_ghost_sched_kern *, ctx, u32, cpu)
{
	return ghost_wake_agent_on_check(cpu);
}

static const struct bpf_func_proto bpf_ghost_wake_agent_proto = {
	.func		= bpf_ghost_wake_agent,
	.gpl_only	= true,
	.ret_type	= RET_INTEGER,
	.arg1_type	= ARG_PTR_TO_CTX,
	.arg2_type	= ARG_ANYTHING,
};

BPF_CALL_4(bpf_ghost_run_gtid, struct bpf_ghost_sched_kern *, ctx, s64, gtid,
	   u32, task_barrier, int, run_flags)
{
	return ghost_run_gtid_on(gtid, task_barrier, run_flags,
				 smp_processor_id());
}

static const struct bpf_func_proto bpf_ghost_run_gtid_proto = {
	.func		= bpf_ghost_run_gtid,
	.gpl_only	= true,
	.ret_type	= RET_INTEGER,
	.arg1_type	= ARG_PTR_TO_CTX,
	.arg2_type	= ARG_ANYTHING,
	.arg3_type	= ARG_ANYTHING,
	.arg4_type	= ARG_ANYTHING,
};

bool ghost_bpf_skip_tick(struct ghost_enclave *e, struct rq *rq)
{
	struct bpf_ghost_sched_kern ctx = {};
	struct bpf_prog *prog;

	lockdep_assert_held(&rq->lock);

	prog = rcu_dereference(e->bpf_tick);
	if (!prog)
		return false;

	/* prog returns 1 if we want a tick on this cpu. */
	return BPF_PROG_RUN(prog, &ctx) != 1;
}

/* Returns true if pick_next_task_ghost should retry its loop. */
bool ghost_bpf_pnt(struct ghost_enclave *e, struct rq *rq, struct rq_flags *rf)
{
	struct bpf_ghost_sched_kern ctx = {};
	struct bpf_prog *prog;
	int ret;

	lockdep_assert_held(&rq->lock);

	rcu_read_lock();
	prog = rcu_dereference(e->bpf_pnt);
	if (!prog) {
		rcu_read_unlock();
		return false;
	}

	/*
	 * BPF programs attached here may call ghost_run_gtid(), which requires
	 * that we not hold any RQ locks.  We are called from
	 * pick_next_task_ghost where it is safe to unlock the RQ.
	 */
	rq_unpin_lock(rq, rf);
	raw_spin_unlock(&rq->lock);

	ret = BPF_PROG_RUN(prog, &ctx);

	raw_spin_lock(&rq->lock);
	rq_repin_lock(rq, rf);

	rcu_read_unlock();
	/* prog returns 1 meaning "retry". */
	return ret == 1;
}

/* netns does this to have a packed array of progs[type].  might do this for the
 * task type only, or maybe for all ghost types.
 */
enum ghost_sched_bpf_attach_type {
	GHOST_SCHED_BPF_INVALID = -1,
	GHOST_SCHED_BPF_TICK = 0,
	GHOST_SCHED_BPF_PNT,
	MAX_SCHED_BPF_ATTACH_TYPE
};

static inline enum ghost_sched_bpf_attach_type
to_ghost_sched_bpf_attach_type(enum bpf_attach_type attach_type)
{
	switch (attach_type) {
	case BPF_GHOST_SCHED_SKIP_TICK:
		return GHOST_SCHED_BPF_TICK;
	case BPF_GHOST_SCHED_PNT:
		return GHOST_SCHED_BPF_PNT;
	default:
		return GHOST_SCHED_BPF_INVALID;
	}
}

int ghost_sched_bpf_prog_attach(const union bpf_attr *attr,
				struct bpf_prog *prog)
{
	enum ghost_sched_bpf_attach_type type;

	if (attr->target_fd || attr->attach_flags)
		return -EINVAL;
	type = to_ghost_sched_bpf_attach_type(attr->attach_type);
	if (type < 0)
		return -EINVAL;

	/* no task attachable types yet */

	return -1;
}

int ghost_sched_bpf_prog_detach(const union bpf_attr *attr,
				enum bpf_prog_type ptype)
{
	struct bpf_prog *prog;
	enum ghost_sched_bpf_attach_type type;

	if (attr->attach_flags)
		return -EINVAL;

	type = to_ghost_sched_bpf_attach_type(attr->attach_type);
	if (type < 0)
		return -EINVAL;

	prog = bpf_prog_get_type(attr->attach_bpf_fd, ptype);
	if (IS_ERR(prog))
		return PTR_ERR(prog);

	if (prog->expected_attach_type != attr->attach_type) {
		bpf_prog_put(prog);
		return -EINVAL;
	}

	/* no task attachable types yet */

	bpf_prog_put(prog);

	return -1;
}

static const struct bpf_func_proto *
ghost_sched_func_proto(enum bpf_func_id func_id, const struct bpf_prog *prog)
{
	switch (func_id) {
	case BPF_FUNC_ghost_wake_agent:
		return &bpf_ghost_wake_agent_proto;
	case BPF_FUNC_ghost_run_gtid:
		switch (prog->expected_attach_type) {
		case BPF_GHOST_SCHED_PNT:
			return &bpf_ghost_run_gtid_proto;
		default:
			return NULL;
		}
	default:
		return bpf_base_func_proto(func_id);
	}
}

static bool ghost_sched_is_valid_access(int off, int size,
					enum bpf_access_type type,
					const struct bpf_prog *prog,
					struct bpf_insn_access_aux *info)
{
	/* The verifier guarantees that size > 0. */
	if (off < 0 || off + size > sizeof(struct bpf_ghost_sched)
	    || off % size)
		return false;

	switch (off) {
	default:
		return false;
	}
}

#define SCHEDULER_ACCESS_FIELD(T, F)					\
	T(BPF_FIELD_SIZEOF(struct bpf_ghost_sched_kern, F),		\
	  si->dst_reg, si->src_reg,					\
	  offsetof(struct bpf_ghost_sched_kern, F))

static u32 ghost_sched_convert_ctx_access(enum bpf_access_type type,
					  const struct bpf_insn *si,
					  struct bpf_insn *insn_buf,
					  struct bpf_prog *prog,
					  u32 *target_size)
{
	struct bpf_insn *insn = insn_buf;

	switch (si->off) {
	default:
		*target_size = 0;
		break;
	}

	*target_size = sizeof(u32);

	return insn - insn_buf;
}

const struct bpf_verifier_ops ghost_sched_verifier_ops = {
	.get_func_proto		= ghost_sched_func_proto,
	.is_valid_access	= ghost_sched_is_valid_access,
	.convert_ctx_access	= ghost_sched_convert_ctx_access,
};

const struct bpf_prog_ops ghost_sched_prog_ops = {
};
#else /* !CONFIG_SCHED_CLASS_GHOST */

const struct bpf_verifier_ops ghost_sched_verifier_ops = {};
const struct bpf_prog_ops ghost_sched_prog_ops = {};

int ghost_sched_bpf_prog_attach(const union bpf_attr *attr,
				struct bpf_prog *prog)
{
	return -EINVAL;
}

int ghost_sched_bpf_prog_detach(const union bpf_attr *attr,
				enum bpf_prog_type ptype)
{
	return -EINVAL;
}

#endif /* !CONFIG_SCHED_CLASS_GHOST */