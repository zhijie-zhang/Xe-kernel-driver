/* SPDX-License-Identifier: MIT */
/*
 * Copyright © 2022 Intel Corporation
 */

#ifndef _XE_BB_H_
#define _XE_BB_H_

#include "xe_bb_types.h"

struct dma_fence;

struct xe_gt;
struct xe_engine;
struct xe_sched_job;

struct xe_bb *xe_bb_new(struct xe_gt *gt, u32 size, bool usm);
struct xe_sched_job *xe_bb_create_job(struct xe_engine *kernel_eng,
				      struct xe_bb *bb);
struct xe_sched_job *xe_bb_create_migration_job(struct xe_engine *kernel_eng,
						struct xe_bb *bb, u64 batch_ofs,
						u32 second_idx);
struct xe_sched_job *xe_bb_create_wa_job(struct xe_engine *wa_eng,
					 struct xe_bb *bb, u64 batch_ofs);
void xe_bb_free(struct xe_bb *bb, struct dma_fence *fence);

#endif
