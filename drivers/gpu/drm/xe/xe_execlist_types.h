/* SPDX-License-Identifier: MIT */
/*
 * Copyright © 2022 Intel Corporation
 */

#ifndef _XE_EXECLIST_TYPES_H_
#define _XE_EXECLIST_TYPES_H_

#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>

#include <drm/gpu_scheduler.h>

struct xe_hw_engine;
struct xe_execlist_engine;

struct xe_execlist_port {
	struct xe_hw_engine *hwe;

	spinlock_t lock;

	struct list_head active[DRM_SCHED_PRIORITY_COUNT];

	u32 last_ctx_id;

	struct xe_execlist_engine *running_exl;

	struct timer_list irq_fail;
};

struct xe_execlist_engine {
	struct xe_engine *engine;

	struct drm_gpu_scheduler sched;

	struct drm_sched_entity entity;

	struct xe_execlist_port *port;

	bool has_run;

	struct work_struct fini_async;

	enum drm_sched_priority active_priority;
	struct list_head active_link;
};

#endif
