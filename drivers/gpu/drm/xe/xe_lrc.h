/* SPDX-License-Identifier: MIT */
/*
 * Copyright © 2021 Intel Corporation
 */
#ifndef _XE_LRC_H_
#define _XE_LRC_H_

#include "xe_lrc_types.h"

struct xe_device;
struct xe_engine;
enum xe_engine_class;
struct xe_hw_engine;
struct xe_vm;

#define LRC_PPHWSP_SCRATCH_ADDR (0x34 * 4)

int xe_lrc_init(struct xe_lrc *lrc, struct xe_hw_engine *hwe,
		struct xe_engine *e, struct xe_vm *vm, u32 ring_size);
void xe_lrc_finish(struct xe_lrc *lrc);

size_t xe_lrc_size(struct xe_device *xe, enum xe_engine_class class);
u32 xe_lrc_pphwsp_offset(struct xe_lrc *lrc);

void xe_lrc_set_ring_head(struct xe_lrc *lrc, u32 head);
u32 xe_lrc_ring_head(struct xe_lrc *lrc);
u32 xe_lrc_ring_space(struct xe_lrc *lrc);
void xe_lrc_write_ring(struct xe_lrc *lrc, const void *data, size_t size);

u32 xe_lrc_ggtt_addr(struct xe_lrc *lrc);
u32 *xe_lrc_regs(struct xe_lrc *lrc);

u32 xe_lrc_read_ctx_reg(struct xe_lrc *lrc, int reg_nr);
void xe_lrc_write_ctx_reg(struct xe_lrc *lrc, int reg_nr, u32 val);

u64 xe_lrc_descriptor(struct xe_lrc *lrc);

u32 xe_lrc_seqno_ggtt_addr(struct xe_lrc *lrc);
struct dma_fence *xe_lrc_create_seqno_fence(struct xe_lrc *lrc);
s32 xe_lrc_seqno(struct xe_lrc *lrc);

u32 xe_lrc_start_seqno_ggtt_addr(struct xe_lrc *lrc);
s32 xe_lrc_start_seqno(struct xe_lrc *lrc);

u32 xe_lrc_parallel_ggtt_addr(struct xe_lrc *lrc);
struct iosys_map xe_lrc_parallel_map(struct xe_lrc *lrc);

size_t xe_lrc_skip_size(struct xe_device *xe);

#endif
