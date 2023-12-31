/* SPDX-License-Identifier: MIT */
/*
 * Copyright © 2022 Intel Corporation
 */

#ifndef _XE_GGTT_TYPES_H_
#define _XE_GGTT_TYPES_H_

#include <drm/drm_mm.h>

struct xe_bo;
struct xe_gt;

struct xe_ggtt {
	struct xe_tile *tile;

	u64 size;

#define XE_GGTT_FLAGS_64K BIT(0)
	unsigned int flags;

	struct xe_bo *scratch;

	struct mutex lock;

	u64 __iomem *gsm;

	struct drm_mm mm;
};

#endif
