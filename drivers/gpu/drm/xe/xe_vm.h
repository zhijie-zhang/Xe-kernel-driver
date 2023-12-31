/* SPDX-License-Identifier: MIT */
/*
 * Copyright © 2021 Intel Corporation
 */

#ifndef _XE_VM_H_
#define _XE_VM_H_

#include "xe_bo_types.h"
#include "xe_macros.h"
#include "xe_map.h"
#include "xe_vm_types.h"

struct drm_device;
struct drm_printer;
struct drm_file;

struct ttm_buffer_object;
struct ttm_validate_buffer;

struct xe_engine;
struct xe_file;
struct xe_sync_entry;

struct xe_vm *xe_vm_create(struct xe_device *xe, u32 flags);
void xe_vm_free(struct kref *ref);

struct xe_vm *xe_vm_lookup(struct xe_file *xef, u32 id);
int xe_vma_cmp_vma_cb(const void *key, const struct rb_node *node);

static inline struct xe_vm *xe_vm_get(struct xe_vm *vm)
{
	kref_get(&vm->refcount);
	return vm;
}

static inline void xe_vm_put(struct xe_vm *vm)
{
	kref_put(&vm->refcount, xe_vm_free);
}

int xe_vm_lock(struct xe_vm *vm, struct ww_acquire_ctx *ww,
	       int num_resv, bool intr);

void xe_vm_unlock(struct xe_vm *vm, struct ww_acquire_ctx *ww);

static inline bool xe_vm_is_closed(struct xe_vm *vm)
{
	/* Only guaranteed not to change when vm->lock is held */
	return !vm->size;
}

static inline bool xe_vm_is_banned(struct xe_vm *vm)
{
	return vm->flags & XE_VM_FLAG_BANNED;
}

static inline bool xe_vm_is_closed_or_banned(struct xe_vm *vm)
{
	lockdep_assert_held(&vm->lock);
	return xe_vm_is_closed(vm) || xe_vm_is_banned(vm);
}

struct xe_vma *
xe_vm_find_overlapping_vma(struct xe_vm *vm, u64 start, u64 range);

static inline struct xe_vm *gpuva_to_vm(struct drm_gpuva *gpuva)
{
	return container_of(gpuva->mgr, struct xe_vm, mgr);
}

static inline struct xe_vma *gpuva_to_vma(struct drm_gpuva *gpuva)
{
	return container_of(gpuva, struct xe_vma, gpuva);
}

static inline struct xe_vma_op *gpuva_op_to_vma_op(struct drm_gpuva_op *op)
{
	return container_of(op, struct xe_vma_op, base);
}

/**
 * DOC: Provide accessors for vma members to facilitate easy change of
 * implementation.
 */
static inline u64 xe_vma_start(struct xe_vma *vma)
{
	return vma->gpuva.va.addr;
}

static inline u64 xe_vma_size(struct xe_vma *vma)
{
	return vma->gpuva.va.range;
}

static inline u64 xe_vma_end(struct xe_vma *vma)
{
	return xe_vma_start(vma) + xe_vma_size(vma);
}

static inline u64 xe_vma_bo_offset(struct xe_vma *vma)
{
	return vma->gpuva.gem.offset;
}

static inline struct xe_bo *xe_vma_bo(struct xe_vma *vma)
{
	return !vma->gpuva.gem.obj ? NULL :
		container_of(vma->gpuva.gem.obj, struct xe_bo, ttm.base);
}

static inline struct xe_vm *xe_vma_vm(struct xe_vma *vma)
{
	return container_of(vma->gpuva.mgr, struct xe_vm, mgr);
}

static inline bool xe_vma_read_only(struct xe_vma *vma)
{
	return vma->gpuva.flags & XE_VMA_READ_ONLY;
}

static inline u64 xe_vma_userptr(struct xe_vma *vma)
{
	return vma->gpuva.gem.offset;
}

static inline bool xe_vma_is_null(struct xe_vma *vma)
{
	return vma->gpuva.flags & DRM_GPUVA_SPARSE;
}

static inline bool xe_vma_has_no_bo(struct xe_vma *vma)
{
	return !xe_vma_bo(vma);
}

static inline bool xe_vma_is_userptr(struct xe_vma *vma)
{
	return xe_vma_has_no_bo(vma) && !xe_vma_is_null(vma);
}

#define xe_vm_assert_held(vm) dma_resv_assert_held(&(vm)->resv)

u64 xe_vm_pdp4_descriptor(struct xe_vm *vm, struct xe_tile *tile);

int xe_vm_create_ioctl(struct drm_device *dev, void *data,
		       struct drm_file *file);
int xe_vm_destroy_ioctl(struct drm_device *dev, void *data,
			struct drm_file *file);
int xe_vm_bind_ioctl(struct drm_device *dev, void *data,
		     struct drm_file *file);

void xe_vm_close_and_put(struct xe_vm *vm);

static inline bool xe_vm_in_compute_mode(struct xe_vm *vm)
{
	return vm->flags & XE_VM_FLAG_COMPUTE_MODE;
}

static inline bool xe_vm_in_fault_mode(struct xe_vm *vm)
{
	return vm->flags & XE_VM_FLAG_FAULT_MODE;
}

static inline bool xe_vm_no_dma_fences(struct xe_vm *vm)
{
	return xe_vm_in_compute_mode(vm) || xe_vm_in_fault_mode(vm);
}

int xe_vm_add_compute_engine(struct xe_vm *vm, struct xe_engine *e);

int xe_vm_userptr_pin(struct xe_vm *vm);

int __xe_vm_userptr_needs_repin(struct xe_vm *vm);

int xe_vm_userptr_check_repin(struct xe_vm *vm);

struct dma_fence *xe_vm_rebind(struct xe_vm *vm, bool rebind_worker);

int xe_vm_invalidate_vma(struct xe_vma *vma);

int xe_vm_async_fence_wait_start(struct dma_fence *fence);

extern struct ttm_device_funcs xe_ttm_funcs;

struct ttm_buffer_object *xe_vm_ttm_bo(struct xe_vm *vm);

/**
 * xe_vm_reactivate_rebind() - Reactivate the rebind functionality on compute
 * vms.
 * @vm: The vm.
 *
 * If the rebind functionality on a compute vm was disabled due
 * to nothing to execute. Reactivate it and run the rebind worker.
 * This function should be called after submitting a batch to a compute vm.
 */
static inline void xe_vm_reactivate_rebind(struct xe_vm *vm)
{
	if (xe_vm_in_compute_mode(vm) && vm->preempt.rebind_deactivated) {
		vm->preempt.rebind_deactivated = false;
		queue_work(system_unbound_wq, &vm->preempt.rebind_work);
	}
}

int xe_vma_userptr_pin_pages(struct xe_vma *vma);

int xe_vma_userptr_check_repin(struct xe_vma *vma);

static inline void xe_vm_queue_rebind_worker(struct xe_vm *vm)
{
	XE_WARN_ON(!xe_vm_in_compute_mode(vm));
	queue_work(vm->xe->ordered_wq, &vm->preempt.rebind_work);
}

/*
 * XE_ONSTACK_TV is used to size the tv_onstack array that is input
 * to xe_vm_lock_dma_resv() and xe_vm_unlock_dma_resv().
 */
#define XE_ONSTACK_TV 20
int xe_vm_lock_dma_resv(struct xe_vm *vm, struct ww_acquire_ctx *ww,
			struct ttm_validate_buffer *tv_onstack,
			struct ttm_validate_buffer **tv,
			struct list_head *objs,
			bool intr,
			unsigned int num_shared);

void xe_vm_unlock_dma_resv(struct xe_vm *vm,
			   struct ttm_validate_buffer *tv_onstack,
			   struct ttm_validate_buffer *tv,
			   struct ww_acquire_ctx *ww,
			   struct list_head *objs);

void xe_vm_fence_all_extobjs(struct xe_vm *vm, struct dma_fence *fence,
			     enum dma_resv_usage usage);

int xe_analyze_vm(struct drm_printer *p, struct xe_vm *vm, int gt_id);

#if IS_ENABLED(CONFIG_DRM_XE_DEBUG_VM)
#define vm_dbg drm_dbg
#else
__printf(2, 3)
static inline void vm_dbg(const struct drm_device *dev,
			  const char *format, ...)
{ /* noop */ }
#endif
#endif
