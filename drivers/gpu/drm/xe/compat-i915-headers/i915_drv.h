/* SPDX-License-Identifier: MIT */
/*
 * Copyright © 2023 Intel Corporation
 */
#ifndef _XE_I915_DRV_H_
#define _XE_I915_DRV_H_

/*
 * "Adaptation header" to allow i915 display to also build for xe driver.
 * TODO: refactor i915 and xe so this can cease to exist
 */

#include "xe_device.h"
#include "xe_bo.h"
#include "xe_pm.h"
#include "xe_step.h"
#include "i915_reg_defs.h"
#include "intel_pch.h"
#include "i915_utils.h"
#include "intel_uncore.h"
#include <linux/pm_runtime.h>

static inline struct drm_i915_private *to_i915(const struct drm_device *dev)
{
	return container_of(dev, struct drm_i915_private, drm);
}

static inline struct drm_i915_private *kdev_to_i915(struct device *kdev)
{
	return dev_get_drvdata(kdev);
}


#define INTEL_JASPERLAKE 0
#define INTEL_ELKHARTLAKE 0
#define IS_PLATFORM(xe, x) ((xe)->info.platform == x)
#define INTEL_INFO(dev_priv)	(&((dev_priv)->info))
#define INTEL_DEVID(dev_priv)	((dev_priv)->info.devid)
#define IS_I830(dev_priv)	(dev_priv && 0)
#define IS_I845G(dev_priv)	(dev_priv && 0)
#define IS_I85X(dev_priv)	(dev_priv && 0)
#define IS_I865G(dev_priv)	(dev_priv && 0)
#define IS_I915G(dev_priv)	(dev_priv && 0)
#define IS_I915GM(dev_priv)	(dev_priv && 0)
#define IS_I945G(dev_priv)	(dev_priv && 0)
#define IS_I945GM(dev_priv)	(dev_priv && 0)
#define IS_I965G(dev_priv)	(dev_priv && 0)
#define IS_I965GM(dev_priv)	(dev_priv && 0)
#define IS_G45(dev_priv)	(dev_priv && 0)
#define IS_GM45(dev_priv)	(dev_priv && 0)
#define IS_G4X(dev_priv)	(dev_priv && 0)
#define IS_PINEVIEW(dev_priv)	(dev_priv && 0)
#define IS_G33(dev_priv)	(dev_priv && 0)
#define IS_IRONLAKE(dev_priv)	(dev_priv && 0)
#define IS_IRONLAKE_M(dev_priv) (dev_priv && 0)
#define IS_SANDYBRIDGE(dev_priv)	(dev_priv && 0)
#define IS_IVYBRIDGE(dev_priv)	(dev_priv && 0)
#define IS_IVB_GT1(dev_priv)	(dev_priv && 0)
#define IS_VALLEYVIEW(dev_priv)	(dev_priv && 0)
#define IS_CHERRYVIEW(dev_priv)	(dev_priv && 0)
#define IS_HASWELL(dev_priv)	(dev_priv && 0)
#define IS_BROADWELL(dev_priv)	(dev_priv && 0)
#define IS_SKYLAKE(dev_priv)	(dev_priv && 0)
#define IS_GEN9_BC(dev_priv)	(dev_priv && 0)
#define IS_GEN9_LP(dev_priv)	(dev_priv && 0)
#define IS_BROXTON(dev_priv)	(dev_priv && 0)
#define IS_KABYLAKE(dev_priv)	(dev_priv && 0)
#define IS_GEMINILAKE(dev_priv)	(dev_priv && 0)
#define IS_COFFEELAKE(dev_priv)	(dev_priv && 0)
#define IS_COMETLAKE(dev_priv)	(dev_priv && 0)
#define IS_ICELAKE(dev_priv)	(dev_priv && 0)
#define IS_JSL_EHL(dev_priv)	(dev_priv && 0)
#define IS_TIGERLAKE(dev_priv)	IS_PLATFORM(dev_priv, XE_TIGERLAKE)
#define IS_ROCKETLAKE(dev_priv)	IS_PLATFORM(dev_priv, XE_ROCKETLAKE)
#define IS_DG1(dev_priv)        IS_PLATFORM(dev_priv, XE_DG1)
#define IS_ALDERLAKE_S(dev_priv) IS_PLATFORM(dev_priv, XE_ALDERLAKE_S)
#define IS_ALDERLAKE_P(dev_priv) (dev_priv && 0)
#define IS_XEHPSDV(dev_priv) (dev_priv && 0)
#define IS_DG2(dev_priv)	IS_PLATFORM(dev_priv, XE_DG2)
#define IS_PONTEVECCHIO(dev_priv) IS_PLATFORM(dev_priv, XE_PVC)
#define IS_METEORLAKE(dev_priv) IS_PLATFORM(dev_priv, XE_METEORLAKE)

#define IS_HSW_ULT(dev_priv) (dev_priv && 0)
#define IS_BDW_ULT(dev_priv) (dev_priv && 0)
#define IS_BDW_ULX(dev_priv) (dev_priv && 0)

#define INTEL_DISPLAY_ENABLED(xe) (HAS_DISPLAY((xe)) && !intel_opregion_headless_sku((xe)))
#define DISPLAY_VER(xe) ((xe)->info.display_runtime.ip.ver)
#define IS_DISPLAY_VER(xe, first, last) ((DISPLAY_VER(xe) >= first && DISPLAY_VER(xe) <= last))
#define IS_GRAPHICS_VER(xe, first, last) \
	((xe)->info.graphics_verx100 >= first * 100 && \
	 (xe)->info.graphics_verx100 <= (last*100 + 99))
#define IS_MOBILE(xe) (xe && 0)
#define HAS_LLC(xe) (!IS_DGFX((xe)))

/* Workarounds not handled yet */
#define IS_DISPLAY_STEP(xe, first, last) ({u8 __step = (xe)->info.step.display; first <= __step && __step <= last;})
#define IS_GRAPHICS_STEP(xe, first, last) ({u8 __step = (xe)->info.step.graphics; first <= __step && __step <= last;})
#define IS_LP(xe) (0)

#define IS_TGL_UY(xe) (xe && 0)
#define IS_CML_ULX(xe) (xe && 0)
#define IS_CFL_ULX(xe) (xe && 0)
#define IS_KBL_ULX(xe) (xe && 0)
#define IS_SKL_ULX(xe) (xe && 0)
#define IS_HSW_ULX(xe) (xe && 0)
#define IS_CML_ULT(xe) (xe && 0)
#define IS_CFL_ULT(xe) (xe && 0)
#define IS_KBL_ULT(xe) (xe && 0)
#define IS_SKL_ULT(xe) (xe && 0)

#define IS_DG1_GRAPHICS_STEP(xe, first, last) (IS_DG1(xe) && IS_GRAPHICS_STEP(xe, first, last))
#define IS_DG2_GRAPHICS_STEP(xe, variant, first, last) \
	((xe)->info.subplatform == XE_SUBPLATFORM_DG2_ ## variant && \
	 IS_GRAPHICS_STEP(xe, first, last))
#define IS_XEHPSDV_GRAPHICS_STEP(xe, first, last) (IS_XEHPSDV(xe) && IS_GRAPHICS_STEP(xe, first, last))

/* XXX: No basedie stepping support yet */
#define IS_PVC_BD_STEP(xe, first, last) (!WARN_ON(1) && IS_PONTEVECCHIO(xe))

#define IS_TGL_DISPLAY_STEP(xe, first, last) (IS_TIGERLAKE(xe) && IS_DISPLAY_STEP(xe, first, last))
#define IS_RKL_DISPLAY_STEP(xe, first, last) (IS_ROCKETLAKE(xe) && IS_DISPLAY_STEP(xe, first, last))
#define IS_DG1_DISPLAY_STEP(xe, first, last) (IS_DG1(xe) && IS_DISPLAY_STEP(xe, first, last))
#define IS_DG2_DISPLAY_STEP(xe, first, last) (IS_DG2(xe) && IS_DISPLAY_STEP(xe, first, last))
#define IS_ADLP_DISPLAY_STEP(xe, first, last) (IS_ALDERLAKE_P(xe) && IS_DISPLAY_STEP(xe, first, last))
#define IS_ADLS_DISPLAY_STEP(xe, first, last) (IS_ALDERLAKE_S(xe) && IS_DISPLAY_STEP(xe, first, last))
#define IS_JSL_EHL_DISPLAY_STEP(xe, first, last) (IS_JSL_EHL(xe) && IS_DISPLAY_STEP(xe, first, last))
#define IS_MTL_DISPLAY_STEP(xe, first, last) (IS_METEORLAKE(xe) && IS_DISPLAY_STEP(xe, first, last))

#define IS_DG2_G10(xe) ((xe)->info.subplatform == XE_SUBPLATFORM_DG2_G10)
#define IS_DG2_G11(xe) ((xe)->info.subplatform == XE_SUBPLATFORM_DG2_G11)
#define IS_DG2_G12(xe) ((xe)->info.subplatform == XE_SUBPLATFORM_DG2_G12)
#define IS_ADLP_RPLU(xe) ((xe)->info.subplatform == XE_SUBPLATFORM_ADLP_RPLU)
#define IS_ICL_WITH_PORT_F(xe) (xe && 0)
#define HAS_FLAT_CCS(xe) (xe_device_has_flat_ccs(xe))
#define HAS_4TILE(xe) ((xe)->info.has_4tile)
#define to_intel_bo(x) gem_to_xe_bo((x))
#define mkwrite_device_info(xe) (INTEL_INFO(xe))

#define HAS_128_BYTE_Y_TILING(xe) (xe || 1)

#define intel_has_gpu_reset(a) (a && 0)

#include "intel_wakeref.h"

static inline bool intel_runtime_pm_get(struct xe_runtime_pm *pm)
{
	struct xe_device *xe = container_of(pm, struct xe_device, runtime_pm);

	if (xe_pm_runtime_get(xe) < 0) {
		xe_pm_runtime_put(xe);
		return false;
	}
	return true;
}

static inline bool intel_runtime_pm_get_if_in_use(struct xe_runtime_pm *pm)
{
	struct xe_device *xe = container_of(pm, struct xe_device, runtime_pm);

	return xe_pm_runtime_get_if_active(xe);
}

static inline void intel_runtime_pm_put_unchecked(struct xe_runtime_pm *pm)
{
	struct xe_device *xe = container_of(pm, struct xe_device, runtime_pm);

	xe_pm_runtime_put(xe);
}

static inline void intel_runtime_pm_put(struct xe_runtime_pm *pm, bool wakeref)
{
	if (wakeref)
		intel_runtime_pm_put_unchecked(pm);
}

#define intel_runtime_pm_get_raw intel_runtime_pm_get
#define intel_runtime_pm_put_raw intel_runtime_pm_put
#define assert_rpm_wakelock_held(x) do { } while (0)
#define assert_rpm_raw_wakeref_held(x) do { } while (0)

#define intel_uncore_forcewake_get(x, y) do { } while (0)
#define intel_uncore_forcewake_put(x, y) do { } while (0)

#define intel_uncore_arm_unclaimed_mmio_detection(x) do { } while (0)
#define i915_sw_fence_commit(x) do { } while (0)

#define with_intel_runtime_pm(rpm, wf) \
	for ((wf) = intel_runtime_pm_get(rpm); (wf); \
	     intel_runtime_pm_put((rpm), (wf)), (wf) = 0)

#define intel_step_name xe_step_name
#define pdev_to_i915 pdev_to_xe_device
#define DISPLAY_INFO(xe)		((xe)->info.display)
#define RUNTIME_INFO(xe)		(&(xe)->info.i915_runtime)
#define DISPLAY_RUNTIME_INFO(xe)	(&(xe)->info.display_runtime)

#define FORCEWAKE_ALL XE_FORCEWAKE_ALL
#define HPD_STORM_DEFAULT_THRESHOLD 50

#ifdef CONFIG_ARM64
/*
 * arm64 indirectly includes linux/rtc.h,
 * which defines a irq_lock, so include it
 * here before #define-ing it
 */
#include <linux/rtc.h>
#endif

#define irq_lock irq.lock

#endif
