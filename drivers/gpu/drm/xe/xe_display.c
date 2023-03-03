// SPDX-License-Identifier: MIT
/*
 * Copyright © 2023 Intel Corporation
 */

#if IS_ENABLED(CONFIG_DRM_XE_DISPLAY)

#include "xe_display.h"

#include <linux/fb.h>

#include <drm/drm_aperture.h>
#include <drm/drm_managed.h>
#include <drm/xe_drm.h>

#include "display/ext/i915_irq.h"
#include "display/ext/intel_dram.h"
#include "display/ext/intel_pm.h"
#include "display/intel_acpi.h"
#include "display/intel_audio.h"
#include "display/intel_bw.h"
#include "display/intel_display.h"
#include "display/intel_display_types.h"
#include "display/intel_dp.h"
#include "display/intel_fbdev.h"
#include "display/intel_hdcp.h"
#include "display/intel_hotplug.h"
#include "display/intel_opregion.h"
#include "xe_module.h"

/* Xe device functions */

/**
 * xe_display_set_driver_hooks - set driver flags and hooks for display
 * @pdev: PCI device
 * @driver: DRM device driver
 *
 * Set features and function hooks in @driver that are needed for driving the
 * display IP, when that is enabled.
 *
 * Returns: 0 on success
 */
int xe_display_set_driver_hooks(struct pci_dev *pdev, struct drm_driver *driver)
{
	if (!enable_display)
		return 0;

	/* Detect if we need to wait for other drivers early on */
	if (intel_modeset_probe_defer(pdev))
		return -EPROBE_DEFER;

	driver->driver_features |= DRIVER_MODESET | DRIVER_ATOMIC;
	driver->lastclose = intel_fbdev_restore_mode;

	return 0;
}

void xe_display_fini_nommio(struct drm_device *dev, void *dummy)
{
	struct xe_device *xe = to_xe_device(dev);

	if (!xe->info.enable_display)
		return;

	intel_power_domains_cleanup(xe);
}

int xe_display_init_nommio(struct xe_device *xe)
{
	int err;

	if (!xe->info.enable_display)
		return 0;

	/* This must be called before any calls to HAS_PCH_* */
	intel_detect_pch(xe);
	intel_display_irq_init(xe);

	err = intel_power_domains_init(xe);
	if (err)
		return err;

	intel_init_display_hooks(xe);

	return drmm_add_action_or_reset(&xe->drm, xe_display_fini_nommio, xe);
}

void xe_display_fini_noirq(struct drm_device *dev, void *dummy)
{
	struct xe_device *xe = to_xe_device(dev);

	if (!xe->info.enable_display)
		return;

	intel_modeset_driver_remove_noirq(xe);
	intel_power_domains_driver_remove(xe);
}

int xe_display_init_noirq(struct xe_device *xe)
{
	int err;

	if (!xe->info.enable_display)
		return 0;

	/* Early display init.. */
	intel_opregion_setup(xe);

	/*
	 * Fill the dram structure to get the system dram info. This will be
	 * used for memory latency calculation.
	 */
	intel_dram_detect(xe);

	intel_bw_init_hw(xe);

	intel_device_info_runtime_init(xe);

	err = drm_aperture_remove_conflicting_pci_framebuffers(to_pci_dev(xe->drm.dev),
							       xe->drm.driver);
	if (err)
		return err;

	err = intel_modeset_init_noirq(xe);
	if (err)
		return err;

	return drmm_add_action_or_reset(&xe->drm, xe_display_fini_noirq, NULL);
}

void xe_display_fini_noaccel(struct drm_device *dev, void *dummy)
{
	struct xe_device *xe = to_xe_device(dev);

	if (!xe->info.enable_display)
		return;

	intel_modeset_driver_remove_nogem(xe);
}

int xe_display_init_noaccel(struct xe_device *xe)
{
	int err;

	if (!xe->info.enable_display)
		return 0;

	err = intel_modeset_init_nogem(xe);
	if (err)
		return err;

	return drmm_add_action_or_reset(&xe->drm, xe_display_fini_noaccel, NULL);
}

int xe_display_init(struct xe_device *xe)
{
	if (!xe->info.enable_display)
		return 0;

	return intel_modeset_init(xe);
}

void xe_display_unlink(struct xe_device *xe)
{
	if (!xe->info.enable_display)
		return;

	/* poll work can call into fbdev, hence clean that up afterwards */
	intel_hpd_poll_fini(xe);
	intel_fbdev_fini(xe);

	intel_hdcp_component_fini(xe);
	intel_audio_deinit(xe);
}

void xe_display_register(struct xe_device *xe)
{
	if (!xe->info.enable_display)
		return;

	intel_display_driver_register(xe);
	intel_register_dsm_handler();
	intel_power_domains_enable(xe);
}

void xe_display_unregister(struct xe_device *xe)
{
	if (!xe->info.enable_display)
		return;

	intel_unregister_dsm_handler();
	intel_power_domains_disable(xe);
	intel_display_driver_unregister(xe);
}

void xe_display_modset_driver_remove(struct xe_device *xe)
{
	if (!xe->info.enable_display)
		return;

	intel_modeset_driver_remove(xe);
}

/* IRQ-related functions */

void xe_display_irq_handler(struct xe_device *xe, u32 master_ctl)
{
	if (!xe->info.enable_display)
		return;

	if (master_ctl & GEN11_DISPLAY_IRQ)
		gen11_display_irq_handler(xe);
}

void xe_display_irq_enable(struct xe_device *xe, u32 gu_misc_iir)
{
	if (!xe->info.enable_display)
		return;

	if (gu_misc_iir & GEN11_GU_MISC_GSE)
		intel_opregion_asle_intr(xe);
}

void xe_display_irq_reset(struct xe_device *xe)
{
	if (!xe->info.enable_display)
		return;

	gen11_display_irq_reset(xe);
}

void xe_display_irq_postinstall(struct xe_device *xe, struct xe_gt *gt)
{
	if (!xe->info.enable_display)
		return;

	if (gt->info.id == XE_GT0)
		gen11_display_irq_postinstall(xe);
}

static void intel_suspend_encoders(struct xe_device *xe)
{
	struct drm_device *dev = &xe->drm;
	struct intel_encoder *encoder;

	if (!xe->info.display.pipe_mask)
		return;

	drm_modeset_lock_all(dev);
	for_each_intel_encoder(dev, encoder)
		if (encoder->suspend)
			encoder->suspend(encoder);
	drm_modeset_unlock_all(dev);
}

void xe_display_pm_suspend(struct xe_device *xe)
{
	if (!xe->info.enable_display)
		return;

	/*
	 * We do a lot of poking in a lot of registers, make sure they work
	 * properly.
	 */
	intel_power_domains_disable(xe);
	if (xe->info.display.pipe_mask)
		drm_kms_helper_poll_disable(&xe->drm);

	intel_display_suspend(&xe->drm);

	intel_dp_mst_suspend(xe);

	intel_hpd_cancel_work(xe);

	intel_suspend_encoders(xe);

	intel_opregion_suspend(xe, PCI_D3cold);

	intel_fbdev_set_suspend(&xe->drm, FBINFO_STATE_SUSPENDED, true);

	intel_dmc_ucode_suspend(xe);
}

void xe_display_pm_suspend_late(struct xe_device *xe)
{
	if (!xe->info.enable_display)
		return;

	intel_power_domains_suspend(xe, I915_DRM_SUSPEND_MEM);

	intel_display_power_suspend_late(xe);
}

void xe_display_pm_resume_early(struct xe_device *xe)
{
	if (!xe->info.enable_display)
		return;

	intel_display_power_resume_early(xe);

	intel_power_domains_resume(xe);
}

void xe_display_pm_resume(struct xe_device *xe)
{
	if (!xe->info.enable_display)
		return;

	intel_dmc_ucode_resume(xe);

	if (xe->info.display.pipe_mask)
		drm_mode_config_reset(&xe->drm);

	intel_modeset_init_hw(xe);
	intel_init_clock_gating(xe);
	intel_hpd_init(xe);

	/* MST sideband requires HPD interrupts enabled */
	intel_dp_mst_resume(xe);
	intel_display_resume(&xe->drm);

	intel_hpd_poll_disable(xe);
	if (xe->info.display.pipe_mask)
		drm_kms_helper_poll_enable(&xe->drm);

	intel_opregion_resume(xe);

	intel_fbdev_set_suspend(&xe->drm, FBINFO_STATE_RUNNING, false);

	intel_power_domains_enable(xe);
}

#endif
