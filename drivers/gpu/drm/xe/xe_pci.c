// SPDX-License-Identifier: MIT
/*
 * Copyright © 2021 Intel Corporation
 */

#include "xe_pci.h"

#include <linux/device/driver.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/pm_runtime.h>

#include <drm/drm_color_mgmt.h>
#include <drm/drm_drv.h>
#include <drm/xe_pciids.h>

#include "regs/xe_regs.h"
#include "regs/xe_gt_regs.h"
#include "xe_device.h"
#include "xe_display.h"
#include "xe_drv.h"
#include "xe_gt.h"
#include "xe_macros.h"
#include "xe_module.h"
#include "xe_pci_types.h"
#include "xe_pm.h"
#include "xe_step.h"

enum toggle_d3cold {
	D3COLD_DISABLE,
	D3COLD_ENABLE,
};

struct xe_subplatform_desc {
	enum xe_subplatform subplatform;
	const char *name;
	const u16 *pciidlist;
};

struct xe_gt_desc {
	enum xe_gt_type type;
	u32 mmio_adj_limit;
	u32 mmio_adj_offset;
};

struct xe_device_desc {
	/* Should only ever be set for platforms without GMD_ID */
	const struct xe_graphics_desc *graphics;
	/* Should only ever be set for platforms without GMD_ID */
	const struct xe_media_desc *media;

	const char *platform_name;
	const struct xe_subplatform_desc *subplatforms;

	enum xe_platform platform;

	u8 require_force_probe:1;
	u8 is_dgfx:1;
	u8 has_display:1;

	/*
	 * FIXME: Xe doesn't care about presence/lack of 4tile since we can
	 * already determine that from the graphics IP version.  This flag
	 * should eventually move entirely into the display code's own logic.
	 */
	u8 has_4tile:1;
	u8 has_llc:1;
};

__diag_push();
__diag_ignore_all("-Woverride-init", "Allow field overrides in table");

#define PLATFORM(x)		\
	.platform = (x),	\
	.platform_name = #x

#define NOP(x)	x

static const struct xe_graphics_desc graphics_xelp = {
	.name = "Xe_LP",
	.ver = 12,
	.rel = 0,

	.hw_engine_mask = BIT(XE_HW_ENGINE_RCS0) | BIT(XE_HW_ENGINE_BCS0),

	.dma_mask_size = 39,
	.vm_max_level = 3,
};

static const struct xe_graphics_desc graphics_xelpp = {
	.name = "Xe_LP+",
	.ver = 12,
	.rel = 10,

	.hw_engine_mask = BIT(XE_HW_ENGINE_RCS0) | BIT(XE_HW_ENGINE_BCS0),

	.dma_mask_size = 39,
	.vm_max_level = 3,
};

#define XE_HP_FEATURES \
	.has_range_tlb_invalidation = true, \
	.has_flat_ccs = true, \
	.dma_mask_size = 46, \
	.vm_max_level = 3

static const struct xe_graphics_desc graphics_xehpg = {
	.name = "Xe_HPG",
	.ver = 12,
	.rel = 55,

	.hw_engine_mask =
		BIT(XE_HW_ENGINE_RCS0) | BIT(XE_HW_ENGINE_BCS0) |
		BIT(XE_HW_ENGINE_CCS0) | BIT(XE_HW_ENGINE_CCS1) |
		BIT(XE_HW_ENGINE_CCS2) | BIT(XE_HW_ENGINE_CCS3),

	XE_HP_FEATURES,
	.vram_flags = XE_VRAM_FLAGS_NEED64K,
};

static const struct xe_graphics_desc graphics_xehpc = {
	.name = "Xe_HPC",
	.ver = 12,
	.rel = 60,

	.hw_engine_mask =
		BIT(XE_HW_ENGINE_BCS0) | BIT(XE_HW_ENGINE_BCS1) |
		BIT(XE_HW_ENGINE_BCS2) | BIT(XE_HW_ENGINE_BCS3) |
		BIT(XE_HW_ENGINE_BCS4) | BIT(XE_HW_ENGINE_BCS5) |
		BIT(XE_HW_ENGINE_BCS6) | BIT(XE_HW_ENGINE_BCS7) |
		BIT(XE_HW_ENGINE_BCS8) |
		BIT(XE_HW_ENGINE_CCS0) | BIT(XE_HW_ENGINE_CCS1) |
		BIT(XE_HW_ENGINE_CCS2) | BIT(XE_HW_ENGINE_CCS3),

	XE_HP_FEATURES,
	.dma_mask_size = 52,
	.max_remote_tiles = 1,
	.vm_max_level = 4,
	.vram_flags = XE_VRAM_FLAGS_NEED64K,

	.has_asid = 1,
	.has_flat_ccs = 0,
	.has_link_copy_engine = 1,
	.supports_usm = 1,
};

static const struct xe_graphics_desc graphics_xelpg = {
	.name = "Xe_LPG",
	.hw_engine_mask =
		BIT(XE_HW_ENGINE_RCS0) | BIT(XE_HW_ENGINE_BCS0) |
		BIT(XE_HW_ENGINE_CCS0),

	XE_HP_FEATURES,
	.has_flat_ccs = 0,
};

static const struct xe_media_desc media_xem = {
	.name = "Xe_M",
	.ver = 12,
	.rel = 0,

	.hw_engine_mask =
		BIT(XE_HW_ENGINE_VCS0) | BIT(XE_HW_ENGINE_VCS2) |
		BIT(XE_HW_ENGINE_VECS0),
};

static const struct xe_media_desc media_xehpm = {
	.name = "Xe_HPM",
	.ver = 12,
	.rel = 55,

	.hw_engine_mask =
		BIT(XE_HW_ENGINE_VCS0) | BIT(XE_HW_ENGINE_VCS2) |
		BIT(XE_HW_ENGINE_VECS0) | BIT(XE_HW_ENGINE_VECS1),
};

static const struct xe_media_desc media_xelpmp = {
	.name = "Xe_LPM+",
	.hw_engine_mask =
		BIT(XE_HW_ENGINE_VCS0) | BIT(XE_HW_ENGINE_VCS2) |
		BIT(XE_HW_ENGINE_VECS0),	/* TODO: add GSC0 */
};

static const struct xe_device_desc tgl_desc = {
	.graphics = &graphics_xelp,
	.media = &media_xem,
	PLATFORM(XE_TIGERLAKE),
	.has_display = true,
	.has_llc = true,
	.require_force_probe = true,
};

static const struct xe_device_desc rkl_desc = {
	.graphics = &graphics_xelp,
	.media = &media_xem,
	PLATFORM(XE_ROCKETLAKE),
	.has_display = true,
	.require_force_probe = true,
};

static const struct xe_device_desc adl_s_desc = {
	.graphics = &graphics_xelp,
	.media = &media_xem,
	PLATFORM(XE_ALDERLAKE_S),
	.has_display = true,
	.has_llc = true,
	.require_force_probe = true,
};

static const u16 adlp_rplu_ids[] = { XE_RPLU_IDS(NOP), 0 };

static const struct xe_device_desc adl_p_desc = {
	.graphics = &graphics_xelp,
	.media = &media_xem,
	PLATFORM(XE_ALDERLAKE_P),
	.has_display = true,
	.has_llc = true,
	.require_force_probe = true,
	.subplatforms = (const struct xe_subplatform_desc[]) {
		{ XE_SUBPLATFORM_ADLP_RPLU, "RPLU", adlp_rplu_ids },
		{},
	},
};

static const struct xe_device_desc adl_n_desc = {
	.graphics = &graphics_xelp,
	.media = &media_xem,
	PLATFORM(XE_ALDERLAKE_N),
	.has_display = true,
	.has_llc = true,
	.require_force_probe = true,
};

#define DGFX_FEATURES \
	.is_dgfx = 1

static const struct xe_device_desc dg1_desc = {
	.graphics = &graphics_xelpp,
	.media = &media_xem,
	DGFX_FEATURES,
	PLATFORM(XE_DG1),
	.has_display = true,
	.require_force_probe = true,
};

static const u16 dg2_g10_ids[] = { XE_DG2_G10_IDS(NOP), XE_ATS_M150_IDS(NOP), 0 };
static const u16 dg2_g11_ids[] = { XE_DG2_G11_IDS(NOP), XE_ATS_M75_IDS(NOP), 0 };
static const u16 dg2_g12_ids[] = { XE_DG2_G12_IDS(NOP), 0 };

#define DG2_FEATURES \
	DGFX_FEATURES, \
	PLATFORM(XE_DG2), \
	.subplatforms = (const struct xe_subplatform_desc[]) { \
		{ XE_SUBPLATFORM_DG2_G10, "G10", dg2_g10_ids }, \
		{ XE_SUBPLATFORM_DG2_G11, "G11", dg2_g11_ids }, \
		{ XE_SUBPLATFORM_DG2_G12, "G12", dg2_g12_ids }, \
		{ } \
	}, \
	.has_4tile = 1

static const struct xe_device_desc ats_m_desc = {
	.graphics = &graphics_xehpg,
	.media = &media_xehpm,
	.require_force_probe = true,

	DG2_FEATURES,
	.has_display = false,
};

static const struct xe_device_desc dg2_desc = {
	.graphics = &graphics_xehpg,
	.media = &media_xehpm,
	.require_force_probe = true,

	DG2_FEATURES,
	.has_display = true,
};

static const struct xe_device_desc pvc_desc = {
	.graphics = &graphics_xehpc,
	DGFX_FEATURES,
	PLATFORM(XE_PVC),
	.has_display = false,
	.require_force_probe = true,
};

static const struct xe_device_desc mtl_desc = {
	/* .graphics and .media determined via GMD_ID */
	.require_force_probe = true,
	PLATFORM(XE_METEORLAKE),
	.has_display = true,
};

#undef PLATFORM
__diag_pop();

/* Map of GMD_ID values to graphics IP */
static struct gmdid_map graphics_ip_map[] = {
	{ 1270, &graphics_xelpg },
	{ 1271, &graphics_xelpg },
};

/* Map of GMD_ID values to media IP */
static struct gmdid_map media_ip_map[] = {
	{ 1300, &media_xelpmp },
};

#define INTEL_VGA_DEVICE(id, info) {			\
	PCI_DEVICE(PCI_VENDOR_ID_INTEL, id),		\
	PCI_BASE_CLASS_DISPLAY << 16, 0xff << 16,	\
	(unsigned long) info }

/*
 * Make sure any device matches here are from most specific to most
 * general.  For example, since the Quanta match is based on the subsystem
 * and subvendor IDs, we need it to come before the more general IVB
 * PCI ID matches, otherwise we'll use the wrong info struct above.
 */
static const struct pci_device_id pciidlist[] = {
	XE_TGL_IDS(INTEL_VGA_DEVICE, &tgl_desc),
	XE_RKL_IDS(INTEL_VGA_DEVICE, &rkl_desc),
	XE_ADLS_IDS(INTEL_VGA_DEVICE, &adl_s_desc),
	XE_ADLP_IDS(INTEL_VGA_DEVICE, &adl_p_desc),
	XE_ADLN_IDS(INTEL_VGA_DEVICE, &adl_n_desc),
	XE_RPLP_IDS(INTEL_VGA_DEVICE, &adl_p_desc),
	XE_DG1_IDS(INTEL_VGA_DEVICE, &dg1_desc),
	XE_ATS_M_IDS(INTEL_VGA_DEVICE, &ats_m_desc),
	XE_DG2_IDS(INTEL_VGA_DEVICE, &dg2_desc),
	XE_PVC_IDS(INTEL_VGA_DEVICE, &pvc_desc),
	XE_MTL_IDS(INTEL_VGA_DEVICE, &mtl_desc),
	{ }
};
MODULE_DEVICE_TABLE(pci, pciidlist);

#undef INTEL_VGA_DEVICE

/* is device_id present in comma separated list of ids */
static bool device_id_in_list(u16 device_id, const char *devices, bool negative)
{
	char *s, *p, *tok;
	bool ret;

	if (!devices || !*devices)
		return false;

	/* match everything */
	if (negative && strcmp(devices, "!*") == 0)
		return true;
	if (!negative && strcmp(devices, "*") == 0)
		return true;

	s = kstrdup(devices, GFP_KERNEL);
	if (!s)
		return false;

	for (p = s, ret = false; (tok = strsep(&p, ",")) != NULL; ) {
		u16 val;

		if (negative && tok[0] == '!')
			tok++;
		else if ((negative && tok[0] != '!') ||
			 (!negative && tok[0] == '!'))
			continue;

		if (kstrtou16(tok, 16, &val) == 0 && val == device_id) {
			ret = true;
			break;
		}
	}

	kfree(s);

	return ret;
}

static bool id_forced(u16 device_id)
{
	return device_id_in_list(device_id, xe_param_force_probe, false);
}

static bool id_blocked(u16 device_id)
{
	return device_id_in_list(device_id, xe_param_force_probe, true);
}

static const struct xe_subplatform_desc *
find_subplatform(const struct xe_device *xe, const struct xe_device_desc *desc)
{
	const struct xe_subplatform_desc *sp;
	const u16 *id;

	for (sp = desc->subplatforms; sp && sp->subplatform; sp++)
		for (id = sp->pciidlist; *id; id++)
			if (*id == xe->info.devid)
				return sp;

	return NULL;
}

static void peek_gmdid(struct xe_device *xe, u32 gmdid_offset, u32 *ver, u32 *revid)
{
	struct pci_dev *pdev = to_pci_dev(xe->drm.dev);
	void __iomem *map = pci_iomap_range(pdev, 0, gmdid_offset, sizeof(u32));
	u32 val;

	if (!map) {
		drm_err(&xe->drm, "Failed to read GMD_ID (%#x) from PCI BAR.\n",
			gmdid_offset);
		*ver = 0;
		*revid = 0;

		return;
	}

	val = ioread32(map);
	pci_iounmap(pdev, map);

	*ver = REG_FIELD_GET(GMD_ID_ARCH_MASK, val) * 100 +
		REG_FIELD_GET(GMD_ID_RELEASE_MASK, val);
	*revid = REG_FIELD_GET(GMD_ID_REVID, val);
}

/*
 * Pre-GMD_ID platform: device descriptor already points to the appropriate
 * graphics descriptor. Simply forward the description and calculate the version
 * appropriately. "graphics" should be present in all such platforms, while
 * media is optional.
 */
static void handle_pre_gmdid(struct xe_device *xe,
			     const struct xe_device_desc *desc,
			     const struct xe_graphics_desc **graphics,
			     const struct xe_media_desc **media)
{
	*graphics = desc->graphics;
	xe->info.graphics_verx100 = (*graphics)->ver * 100 + (*graphics)->rel;

	*media = desc->media;
	if (*media)
		xe->info.media_verx100 = (*media)->ver * 100 + (*media)->rel;

}

/*
 * GMD_ID platform: read IP version from hardware and select graphics descriptor
 * based on the result.
 */
static void handle_gmdid(struct xe_device *xe,
			 const struct xe_device_desc *desc,
			 const struct xe_graphics_desc **graphics,
			 const struct xe_media_desc **media,
			 u32 *graphics_revid,
			 u32 *media_revid)
{
	u32 ver;

	peek_gmdid(xe, GMD_ID.addr, &ver, graphics_revid);
	for (int i = 0; i < ARRAY_SIZE(graphics_ip_map); i++) {
		if (ver == graphics_ip_map[i].ver) {
			xe->info.graphics_verx100 = ver;
			*graphics = graphics_ip_map[i].ip;

			break;
		}
	}

	if (!xe->info.graphics_verx100) {
		drm_err(&xe->drm, "Hardware reports unknown graphics version %u.%02u\n",
			ver / 100, ver % 100);
	}

	peek_gmdid(xe, GMD_ID.addr + 0x380000, &ver, media_revid);

	/* Media may legitimately be fused off / not present */
	if (ver == 0)
		return;

	for (int i = 0; i < ARRAY_SIZE(media_ip_map); i++) {
		if (ver == media_ip_map[i].ver) {
			xe->info.media_verx100 = ver;
			*media = media_ip_map[i].ip;

			break;
		}
	}

	if (!xe->info.media_verx100) {
		drm_err(&xe->drm, "Hardware reports unknown media version %u.%02u\n",
			ver / 100, ver % 100);
	}
}


static int xe_info_init(struct xe_device *xe,
			const struct xe_device_desc *desc,
			const struct xe_subplatform_desc *subplatform_desc)
{
	const struct xe_graphics_desc *graphics_desc = NULL;
	const struct xe_media_desc *media_desc = NULL;
	u32 graphics_gmdid_revid = 0, media_gmdid_revid = 0;
	struct xe_tile *tile;
	struct xe_gt *gt;
	u8 id;

	xe->info.platform = desc->platform;
	xe->info.subplatform = subplatform_desc ?
		subplatform_desc->subplatform : XE_SUBPLATFORM_NONE;

	/*
	 * If this platform supports GMD_ID, we'll detect the proper IP
	 * descriptor to use from hardware registers. desc->graphics will only
	 * ever be set at this point for platforms before GMD_ID. In that case
	 * the IP descriptions and versions are simply derived from that.
	 */
	if (desc->graphics) {
		handle_pre_gmdid(xe, desc, &graphics_desc, &media_desc);
		xe->info.step = xe_step_pre_gmdid_get(xe);
	} else {
		handle_gmdid(xe, desc, &graphics_desc, &media_desc,
			     &graphics_gmdid_revid, &media_gmdid_revid);
		xe->info.step = xe_step_gmdid_get(xe,
						  graphics_gmdid_revid,
						  media_gmdid_revid);
	}

	/*
	 * If we couldn't detect the graphics IP, that's considered a fatal
	 * error and we should abort driver load.  Failing to detect media
	 * IP is non-fatal; we'll just proceed without enabling media support.
	 */
	if (!graphics_desc)
		return -ENODEV;

	xe->info.is_dgfx = desc->is_dgfx;
	xe->info.graphics_name = graphics_desc->name;
	xe->info.media_name = media_desc ? media_desc->name : "none";
	xe->info.has_4tile = desc->has_4tile;
	xe->info.has_llc = desc->has_llc;

	xe->info.dma_mask_size = graphics_desc->dma_mask_size;
	xe->info.vram_flags = graphics_desc->vram_flags;
	xe->info.vm_max_level = graphics_desc->vm_max_level;
	xe->info.supports_usm = graphics_desc->supports_usm;
	xe->info.has_asid = graphics_desc->has_asid;
	xe->info.has_flat_ccs = graphics_desc->has_flat_ccs;
	xe->info.has_range_tlb_invalidation = graphics_desc->has_range_tlb_invalidation;
	xe->info.has_link_copy_engine = graphics_desc->has_link_copy_engine;

	xe->info.enable_display = IS_ENABLED(CONFIG_DRM_XE_DISPLAY) &&
				  enable_display &&
				  desc->has_display;
	/*
	 * All platforms have at least one primary GT.  Any platform with media
	 * version 13 or higher has an additional dedicated media GT.  And
	 * depending on the graphics IP there may be additional "remote tiles."
	 * All of these together determine the overall GT count.
	 *
	 * FIXME: 'tile_count' here is misnamed since the rest of the driver
	 * treats it as the number of GTs rather than just the number of tiles.
	 */
	xe->info.tile_count = 1 + graphics_desc->max_remote_tiles;

	for_each_tile(tile, xe, id) {
		tile->xe = xe;
		tile->id = id;

		tile->primary_gt = xe_gt_alloc(tile);
		if (IS_ERR(tile->primary_gt))
			return PTR_ERR(tile->primary_gt);

		gt = tile->primary_gt;
		/*
		 * FIXME: GT numbering scheme may change depending on UAPI
		 * decisions.
		 */
		gt->info.id = xe->info.gt_count++;
		gt->info.type = XE_GT_TYPE_MAIN;
		gt->info.__engine_mask = graphics_desc->hw_engine_mask;
		if (MEDIA_VER(xe) < 13 && media_desc)
			gt->info.__engine_mask |= media_desc->hw_engine_mask;

		if (MEDIA_VER(xe) < 13 || !media_desc)
			continue;

		/*
		 * Allocate and setup media GT for platforms with standalone
		 * media.
		 */
		tile->media_gt = xe_gt_alloc(tile);
		if (IS_ERR(tile->media_gt))
			return PTR_ERR(tile->media_gt);

		gt = tile->media_gt;
		gt->info.type = XE_GT_TYPE_MEDIA;
		gt->info.__engine_mask = media_desc->hw_engine_mask;
		gt->mmio.adj_offset = MEDIA_GT_GSI_OFFSET;
		gt->mmio.adj_limit = MEDIA_GT_GSI_LENGTH;

		/*
		 * FIXME: At the moment multi-tile and standalone media are
		 * mutually exclusive on current platforms.  We'll need to
		 * come up with a better way to number GTs if we ever wind
		 * up with platforms that support both together.
		 */
		drm_WARN_ON(&xe->drm, id != 0);
		gt->info.id = xe->info.gt_count++;
	}

	return 0;
}

static void xe_pci_remove(struct pci_dev *pdev)
{
	struct xe_device *xe;

	xe = pci_get_drvdata(pdev);
	if (!xe) /* driver load aborted, nothing to cleanup */
		return;

	xe_device_remove(xe);
	xe_pm_runtime_fini(xe);
	pci_set_drvdata(pdev, NULL);
}

static int xe_pci_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	const struct xe_device_desc *desc = (const void *)ent->driver_data;
	const struct xe_subplatform_desc *subplatform_desc;
	struct xe_device *xe;
	int err;

	if (desc->require_force_probe && !id_forced(pdev->device)) {
		dev_info(&pdev->dev,
			 "Your graphics device %04x is not officially supported\n"
			 "by xe driver in this kernel version. To force Xe probe,\n"
			 "use xe.force_probe='%04x' and i915.force_probe='!%04x'\n"
			 "module parameters or CONFIG_DRM_XE_FORCE_PROBE='%04x' and\n"
			 "CONFIG_DRM_I915_FORCE_PROBE='!%04x' configuration options.\n",
			 pdev->device, pdev->device, pdev->device,
			 pdev->device, pdev->device);
		return -ENODEV;
	}

	if (id_blocked(pdev->device)) {
		dev_info(&pdev->dev, "Probe blocked for device [%04x:%04x].\n",
			 pdev->vendor, pdev->device);
		return -ENODEV;
	}

	err = xe_display_driver_probe_defer(pdev);
	if (err)
		return err;

	xe = xe_device_create(pdev, ent);
	if (IS_ERR(xe))
		return PTR_ERR(xe);

	xe_pm_assert_unbounded_bridge(xe);
	subplatform_desc = find_subplatform(xe, desc);

	pci_set_drvdata(pdev, xe);
	err = pci_enable_device(pdev);
	if (err)
		goto err_drm_put;

	pci_set_master(pdev);

	if (pci_enable_msi(pdev) < 0)
		drm_dbg(&xe->drm, "can't enable MSI");

	err = xe_info_init(xe, desc, subplatform_desc);
	if (err)
		goto err_pci_disable;

	xe_display_info_init(xe);

	drm_dbg(&xe->drm, "%s %s %04x:%04x dgfx:%d gfx:%s (%d.%02d) media:%s (%d.%02d) display:%s dma_m_s:%d tc:%d",
		desc->platform_name,
		subplatform_desc ? subplatform_desc->name : "",
		xe->info.devid, xe->info.revid,
		xe->info.is_dgfx,
		xe->info.graphics_name,
		xe->info.graphics_verx100 / 100,
		xe->info.graphics_verx100 % 100,
		xe->info.media_name,
		xe->info.media_verx100 / 100,
		xe->info.media_verx100 % 100,
		str_yes_no(xe->info.enable_display),
		xe->info.dma_mask_size, xe->info.tile_count);

	drm_dbg(&xe->drm, "Stepping = (G:%s, M:%s, D:%s, B:%s)\n",
		xe_step_name(xe->info.step.graphics),
		xe_step_name(xe->info.step.media),
		xe_step_name(xe->info.step.display),
		xe_step_name(xe->info.step.basedie));

	err = xe_device_probe(xe);
	if (err)
		goto err_pci_disable;

	xe_pm_init(xe);

	return 0;

err_pci_disable:
	pci_disable_device(pdev);

err_drm_put:
	drm_dev_put(&xe->drm);

	return err;
}

static void xe_pci_shutdown(struct pci_dev *pdev)
{
	xe_device_shutdown(pdev_to_xe_device(pdev));
}

#ifdef CONFIG_PM_SLEEP
static int xe_pci_suspend(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	int err;

	err = xe_pm_suspend(pdev_to_xe_device(pdev));
	if (err)
		return err;

	pci_save_state(pdev);
	pci_disable_device(pdev);

	err = pci_set_power_state(pdev, PCI_D3hot);
	if (err)
		return err;

	return 0;
}

static int xe_pci_resume(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	int err;

	err = pci_set_power_state(pdev, PCI_D0);
	if (err)
		return err;

	pci_restore_state(pdev);

	err = pci_enable_device(pdev);
	if (err)
		return err;

	pci_set_master(pdev);

	err = xe_pm_resume(pdev_to_xe_device(pdev));
	if (err)
		return err;

	return 0;
}

static void d3cold_toggle(struct pci_dev *pdev, enum toggle_d3cold toggle)
{
	struct xe_device *xe = pdev_to_xe_device(pdev);
	struct pci_dev *root_pdev;

	if (!xe->d3cold.capable)
		return;

	root_pdev = pcie_find_root_port(pdev);
	if (!root_pdev)
		return;

	switch (toggle) {
	case D3COLD_DISABLE:
		pci_d3cold_disable(root_pdev);
		break;
	case D3COLD_ENABLE:
		pci_d3cold_enable(root_pdev);
		break;
	}
}

static int xe_pci_runtime_suspend(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct xe_device *xe = pdev_to_xe_device(pdev);
	int err;

	err = xe_pm_runtime_suspend(xe);
	if (err)
		return err;

	pci_save_state(pdev);

	if (xe->d3cold.allowed) {
		pci_disable_device(pdev);
		pci_ignore_hotplug(pdev);
		pci_set_power_state(pdev, PCI_D3cold);
	} else {
		d3cold_toggle(pdev, D3COLD_DISABLE);
		pci_set_power_state(pdev, PCI_D3hot);
	}

	return 0;
}

static int xe_pci_runtime_resume(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct xe_device *xe = pdev_to_xe_device(pdev);
	int err;

	err = pci_set_power_state(pdev, PCI_D0);
	if (err)
		return err;

	pci_restore_state(pdev);

	if (xe->d3cold.allowed) {
		err = pci_enable_device(pdev);
		if (err)
			return err;

		pci_set_master(pdev);
	} else {
		d3cold_toggle(pdev, D3COLD_ENABLE);
	}

	return xe_pm_runtime_resume(xe);
}

static int xe_pci_runtime_idle(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct xe_device *xe = pdev_to_xe_device(pdev);

	if (!xe->d3cold.capable) {
		xe->d3cold.allowed = false;
	} else {
		xe_pm_d3cold_allowed_toggle(xe);

		/*
		 * TODO: d3cold should be allowed (true) if
		 * (IS_DGFX(xe) && !xe_device_mem_access_ongoing(xe))
		 * but maybe include some other conditions. So, before
		 * we can re-enable the D3cold, we need to:
		 * 1. rewrite the VRAM save / restore to avoid buffer object locks
		 */
		xe->d3cold.allowed = false;
	}

	return 0;
}
#endif

static const struct dev_pm_ops xe_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(xe_pci_suspend, xe_pci_resume)
	SET_RUNTIME_PM_OPS(xe_pci_runtime_suspend, xe_pci_runtime_resume, xe_pci_runtime_idle)
};

static struct pci_driver xe_pci_driver = {
	.name = DRIVER_NAME,
	.id_table = pciidlist,
	.probe = xe_pci_probe,
	.remove = xe_pci_remove,
	.shutdown = xe_pci_shutdown,
	.driver.pm = &xe_pm_ops,
};

int xe_register_pci_driver(void)
{
	return pci_register_driver(&xe_pci_driver);
}

void xe_unregister_pci_driver(void)
{
	pci_unregister_driver(&xe_pci_driver);
}

#if IS_ENABLED(CONFIG_DRM_XE_KUNIT_TEST)
#include "tests/xe_pci.c"
#endif
