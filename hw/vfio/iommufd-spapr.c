/*
 * IOMMUFD PPC64 DMA window management
 *
 * Authors:
 *  Shivaprasad G Bhat <sbhat@linux.ibm.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include <sys/ioctl.h>
#include <linux/vfio.h>
#include <linux/iommufd.h>
#include "vfio-iommufd.h"
#include "system/iommufd.h"
#include "system/hostmem.h"
#include "qemu/error-report.h"
#include "trace.h"
#include "system/kvm.h"
//#include "spapr.c"
#include "hw/vfio/vfio-iommufd-spapr.h"
#include "hw/vfio/vfio-device.h"
#include "hw/core/hw-error.h"

#include "hw/vfio/kvm-spapr.h"

static VFIOHostDMAWindow *vfio_find_hostwin(VFIOIOMMUFDSpaprContainer *container,
                                            hwaddr iova, hwaddr end)
{
    VFIOHostDMAWindow *hostwin;
    bool hostwin_found = false;

    QLIST_FOREACH(hostwin, &container->hostwin_list, hostwin_next) {
        if (hostwin->min_iova <= iova && end <= hostwin->max_iova) {
            hostwin_found = true;
            break;
        }
    }

    return hostwin_found ? hostwin : NULL;
}

static void vfio_host_win_add(VFIOIOMMUFDSpaprContainer *scontainer, hwaddr min_iova,
                              hwaddr max_iova, uint64_t iova_pgsizes)
{
    VFIOHostDMAWindow *hostwin;

    QLIST_FOREACH(hostwin, &scontainer->hostwin_list, hostwin_next) {
        if (ranges_overlap(hostwin->min_iova,
                           hostwin->max_iova - hostwin->min_iova + 1,
                           min_iova,
                           max_iova - min_iova + 1)) {
            hw_error("%s: Overlapped IOMMU are not enabled", __func__);
        }
    }
    warn_report("Adding region [0x%"PRIx64",0x%"PRIx64"]", min_iova, max_iova);

    hostwin = g_malloc0(sizeof(*hostwin));

    hostwin->min_iova = min_iova;
    hostwin->max_iova = max_iova;
    hostwin->iova_pgsizes = iova_pgsizes;
    QLIST_INSERT_HEAD(&scontainer->hostwin_list, hostwin, hostwin_next);
}

static int vfio_host_win_del(VFIOIOMMUFDSpaprContainer *scontainer,
                             hwaddr min_iova, hwaddr max_iova)
{
    VFIOHostDMAWindow *hostwin;

    QLIST_FOREACH(hostwin, &scontainer->hostwin_list, hostwin_next) {
        if (hostwin->min_iova == min_iova && hostwin->max_iova == max_iova) {
            QLIST_REMOVE(hostwin, hostwin_next);
            g_free(hostwin);
            return 0;
        }
    }

    return -1;
}

static bool iommufd_spapr_remove_window(VFIOIOMMUFDSpaprContainer *scontainer,
                                        hwaddr offset_within_address_space,
                                        Error **errp)
{
    VFIOIOMMUFDContainer *container = &scontainer->parent_obj;
    VFIOIOASHwpt *hwpt, *hwpt_to_remove = NULL;
    VFIODevice *vbasedev, *tmp_dev;
    HostIOMMUDeviceIOMMUFD *hiod;
    Error *local_err = NULL;
    bool ret = true;

    /*
     * For SPAPR, we need to remove the HWPT associated with this window.
     * Find the HWPT for this window - it will have devices attached.
     * TODO: Match by offset_within_address_space when we track it per HWPT.
     */
    QLIST_FOREACH(hwpt, &container->hwpt_list, next) {
        if (!QLIST_EMPTY(&hwpt->device_list)) {
            hwpt_to_remove = hwpt;
            break;
        }
    }

    if (!hwpt_to_remove) {
        /* No HWPT with devices found, nothing to remove */
        trace_vfio_spapr_remove_window(offset_within_address_space);
        return true;
    }

    /*
     * Detach all devices from this HWPT before destroying it.
     * Similar to setup where we remove the default window, devices
     * are left detached so new windows can be created on-demand.
     * Unlike legacy VFIO where VFIO_IOMMU_SPAPR_TCE_REMOVE handles
     * everything in the kernel, with iommufd we must explicitly
     * detach devices via host_iommu_device_iommufd_detach_hwpt()
     * before destroying the HWPT.
     */
    QLIST_FOREACH_SAFE(vbasedev, &hwpt_to_remove->device_list, hwpt_next, tmp_dev) {
        if (!vbasedev->hiod) {
            continue;
        }

        hiod = HOST_IOMMU_DEVICE_IOMMUFD(vbasedev->hiod);
        if (!host_iommu_device_iommufd_detach_hwpt(hiod, &local_err)) {
            error_report_err(local_err);
            local_err = NULL;
            ret = false;
            /* Continue trying to detach other devices */
        }
    }

    /* Now safe to remove and free the HWPT */
    QLIST_REMOVE(hwpt_to_remove, next);
    iommufd_backend_free_id(container->be, hwpt_to_remove->hwpt_id);
    g_free(hwpt_to_remove);

    trace_vfio_spapr_remove_window(offset_within_address_space);

    return ret;
}


static bool iommufd_spapr_create_window(VFIOContainer *container,
                                    MemoryRegionSection *section,
                                    hwaddr *pgsize, Error **errp)
{
    int ret = 0;
    VFIOIOMMUFDContainer *bcontainer = VFIO_IOMMU_IOMMUFD(container);
    VFIOIOMMUFDSpaprContainer *scontainer = VFIO_IOMMU_SPAPR_IOMMUFD(bcontainer);
    IOMMUMemoryRegion *iommu_mr = IOMMU_MEMORY_REGION(section->mr);
    uint64_t pagesize = memory_region_iommu_get_min_page_size(iommu_mr), pgmask;
    unsigned entries, bits_total, bits_per_level, max_levels, ddw_levels;
    struct iommu_hwpt_ppc64_dma_window window;
    long rampagesize = qemu_minrampagesize();
    struct iommu_iova_range *iova_ranges = NULL;
    uint32_t num_ranges;
    VFIODevice *vbasedev = NULL;
    uint32_t new_hwpt_id;

    memset(&window, 0, sizeof(window));

    /*
     * Use the container's device_list instead of searching hwpt device lists.
     * This allows us to find devices even when they're detached from HWPTs
     * during window transitions, and handles hot-unplug cleanly since devices
     * are removed from the container list regardless of HWPT state.
     */
    if (!QLIST_EMPTY(&container->device_list)) {
        vbasedev = QLIST_FIRST(&container->device_list);
    }

    if (!vbasedev) {
        error_setg(errp, "No device available for DMA window creation");
        return false;
    }

    /*
     * The host might not support the guest supported IOMMU page size,
     * so we will use smaller physical IOMMU pages to back them.
     */
    if (pagesize > rampagesize) {
        pagesize = rampagesize;
    }
    pgmask = scontainer->pgsizes & (pagesize | (pagesize - 1));
    pagesize = pgmask ? (1ULL << (63 - clz64(pgmask))) : 0;
    if (!pagesize) {
        error_setg_errno(errp, EINVAL, "Host doesn't support page size 0x%"PRIx64
                         ", the supported mask is 0x%lx",
                         memory_region_iommu_get_min_page_size(iommu_mr),
                         scontainer->pgsizes);
        return false;
    }

    /*
     * FIXME: For VFIO iommu types which have KVM acceleration to
     * avoid bouncing all map/unmaps through qemu this way, this
     * would be the right place to wire that up (tell the KVM
     * device emulation the VFIO iommu handles to use).
     */
    window.window_size = int128_get64(section->size);
    window.page_shift = ctz64(pagesize);
    /*
     * SPAPR host supports multilevel TCE tables. We try to guess optimal
     * levels number and if this fails (for example due to the host memory
     * fragmentation), we increase levels. The DMA address structure is:
     * rrrrrrrr rxxxxxxx xxxxxxxx xxxxxxxx  xxxxxxxx xxxxxxxx xxxxxxxx iiiiiiii
     * where:
     *   r = reserved (bits >= 55 are reserved in the existing hardware)
     *   i = IOMMU page offset (64K in this example)
     *   x = bits to index a TCE which can be split to equal chunks to index
     *      within the level.
     * The aim is to split "x" to smaller possible number of levels.
     */
    entries = window.window_size >> window.page_shift;
    /* bits_total is number of "x" needed */
    bits_total = ctz64(entries * sizeof(uint64_t));
    /*
     * bits_per_level is a safe guess of how much we can allocate per level:
     * 8 is the current minimum for CONFIG_FORCE_MAX_ZONEORDER and MAX_ORDER
     * is usually bigger than that.
     * Below we look at qemu_real_host_page_size as TCEs are allocated from
     * system pages.
     */
    bits_per_level = ctz64(qemu_real_host_page_size()) + 8;
    window.levels = bits_total / bits_per_level;

    ddw_levels = scontainer->levels;
    if (ddw_levels > 1) {
        if (bits_total % bits_per_level) {
            ++window.levels;
        }
        max_levels = (64 - window.page_shift) / ctz64(qemu_real_host_page_size());
        for ( ; window.levels <= max_levels; ++window.levels) {
	    ret = iommufd_backend_alloc_hwpt(bcontainer->be, vbasedev->devid,
					bcontainer->ioas_id,
		   		     0, IOMMU_HWPT_DATA_PPC64_DMA_WINDOW,
				     sizeof(window), &window, &new_hwpt_id, errp);
            if (ret) {
                break;
            }
        }
    } else { /* ddw_levels == 1 */
	warn_report("%s: Going to take the levels path\n", __func__);
        if (window.levels > ddw_levels) {
            error_setg_errno(errp, EINVAL, "Host doesn't support multi-level TCE tables"
                             ". Use larger IO page size. Supported mask is 0x%lx",
                             scontainer->pgsizes);
            return false;
        }

	ret = iommufd_backend_alloc_hwpt(bcontainer->be, vbasedev->devid,
					bcontainer->ioas_id,
		   		     0, IOMMU_HWPT_DATA_PPC64_DMA_WINDOW,
				     sizeof(window), &window, &new_hwpt_id, errp);
	warn_report("%s: iommufd_backend_alloc_hwpt returned %d\n", __func__, ret);
    }

    if (!ret) {
        error_setg_errno(errp, errno, "Failed to create a window, ret = %d", ret);
        return false;
    }

    /*
     * Attach all devices in the container to this new HWPT.
     * Use the container's device_list to find all devices, regardless of
     * their current HWPT attachment state (they may be detached).
     */
    QLIST_FOREACH(vbasedev, &container->device_list, container_next) {
        HostIOMMUDeviceIOMMUFD *hiod;

        if (!vbasedev->hiod) {
            continue;
        }

        hiod = HOST_IOMMU_DEVICE_IOMMUFD(vbasedev->hiod);
        ret = host_iommu_device_iommufd_attach_hwpt(hiod, new_hwpt_id, errp);
        if (!ret) {
            error_prepend(errp, "Failed to attach device to new HWPT: ");
            iommufd_backend_free_id(bcontainer->be, new_hwpt_id);
            return false;
        }
    }

    ret = iommufd_backend_get_iova_ranges(bcontainer->be, bcontainer->ioas_id,
                                          &iova_ranges, &num_ranges, errp);
    if (!ret) {
        error_prepend(errp, "Failed to query IOVA ranges after window creation: ");
        iommufd_backend_free_id(bcontainer->be, new_hwpt_id);
        return false;
    }

    for (uint32_t i = 0; i < num_ranges; i++) {
        hwaddr range_start = iova_ranges[i].start;
        hwaddr range_end = iova_ranges[i].last;
	warn_report("%s: The ranges : start is %lx, end is %lx\n", __func__, range_start, range_end);

        /* Skip ranges that already exist in our tracking */
        if (vfio_find_hostwin(scontainer, range_start, range_end)) {
            continue;
        }

        vfio_host_win_add(scontainer, range_start, range_end, pagesize);
        trace_iommufd_spapr_create_window(range_start, range_end, pagesize,
                                       window.levels, new_hwpt_id);
    }

    g_free(iova_ranges);

    *pgsize = pagesize;

    return true;
}

static bool
iommufd_spapr_container_add_section_window(VFIOContainer *bcontainer,
                                        MemoryRegionSection *section,
                                        Error **errp)
{
    VFIOIOMMUFDContainer *container = VFIO_IOMMU_IOMMUFD(bcontainer);
    VFIOIOMMUFDSpaprContainer *scontainer = VFIO_IOMMU_SPAPR_IOMMUFD(container);
    VFIOHostDMAWindow *hostwin;
    hwaddr pgsize = 0;
    int ret;

    /* For now intersections are not allowed, we may relax this later */
    QLIST_FOREACH(hostwin, &scontainer->hostwin_list, hostwin_next) {
        if (ranges_overlap(hostwin->min_iova,
                           hostwin->max_iova - hostwin->min_iova + 1,
                           section->offset_within_address_space,
                           int128_get64(section->size))) {
            error_setg(errp,
                "region [0x%"PRIx64",0x%"PRIx64"] overlaps with existing"
                "host DMA window [0x%"PRIx64",0x%"PRIx64"]",
                section->offset_within_address_space,
                section->offset_within_address_space +
                    int128_get64(section->size) - 1,
                hostwin->min_iova, hostwin->max_iova);
            return false;
        }
    }

    ret = iommufd_spapr_create_window(bcontainer, section, &pgsize, errp);
    if (!ret) {
        return false;
    }

    if (kvm_enabled() && !vfio_spapr_kvm_attach_tce(bcontainer, section, errp)) {
        return false;
    }

    return true;
}

static void
iommufd_spapr_container_del_section_window(VFIOContainer *bcontainer,
                                        MemoryRegionSection *section)
{
    VFIOIOMMUFDContainer *container = VFIO_IOMMU_IOMMUFD(bcontainer);
    VFIOIOMMUFDSpaprContainer *scontainer = VFIO_IOMMU_SPAPR_IOMMUFD(container);
    Error *err = NULL;

    if (!iommufd_spapr_remove_window(scontainer,
                                     section->offset_within_address_space,
                                     &err)) {
        error_report_err(err);
    }

    if (vfio_host_win_del(scontainer,
                          section->offset_within_address_space,
                          section->offset_within_address_space +
                          int128_get64(section->size) - 1) < 0) {
        hw_error("%s: Cannot delete missing window at %"HWADDR_PRIx,
                 __func__, section->offset_within_address_space);
    }
}

static void iommufd_spapr_container_release(VFIOContainer *bcontainer)
{
    VFIOIOMMUFDContainer *icontainer = VFIO_IOMMU_IOMMUFD(bcontainer);
    VFIOIOMMUFDSpaprContainer *scontainer = VFIO_IOMMU_SPAPR_IOMMUFD(icontainer);
    VFIOHostDMAWindow *hostwin, *next;

    QLIST_FOREACH_SAFE(hostwin, &scontainer->hostwin_list, hostwin_next,
                       next) {
        QLIST_REMOVE(hostwin, hostwin_next);
        g_free(hostwin);
    }
}

static bool vfio_spapr_iommufd_container_setup(VFIOContainer *bcontainer,
                                       Error **errp)
{
    VFIOIOMMUFDSpaprContainer *scontainer = VFIO_IOMMU_SPAPR_IOMMUFD(bcontainer);
    struct iommu_hw_info_spapr_tce info;
    VFIODevice *vbasedev = NULL;
    uint32_t data_type;
    uint64_t caps;
    int ret;

    /*
     * Use the container's device_list to find a device for querying hardware info.
     * This is consistent with window creation and handles device lifecycle properly.
     */
    if (!QLIST_EMPTY(&bcontainer->device_list)) {
        vbasedev = QLIST_FIRST(&bcontainer->device_list);
    }

    if (!vbasedev) {
        error_setg(errp, "No devices attached to container");
        return false;
    }

    if (!iommufd_backend_get_device_info(vbasedev->iommufd, vbasedev->devid,
       		 &data_type, &info, sizeof(info), &caps, NULL, errp)) {
        error_prepend(errp, "Failed to get SPAPR TCE info from iommufd (devid=%u): ", vbasedev->devid);
        return false;
    }

    if (data_type != IOMMU_HW_INFO_TYPE_PPC64) {
    	error_setg(errp, "Wrong data type %d for Host SPAPR tce info", data_type);
	return false;
    }

    QLIST_INIT(&scontainer->hostwin_list);
    scontainer->tce32_start = info.tce32_start;
    scontainer->tce32_size = info.tce32_size;
    scontainer->levels = info.max_levels;
    scontainer->max_dynamic_windows_supported = info.max_dynamic_windows_supported;
    warn_report("Initialised\n");

    scontainer->pgsizes = info.pgsizes;

    vfio_host_win_add(scontainer, info.tce32_start,
                      info.tce32_start + info.tce32_size - 1,
                      1ULL << 12);

    /*
     * There is a default window in just created container.
     * To make region_add/del simpler, we better remove this
     * window now and let those iommu_listener callbacks
     * create/remove them when needed.
     */
    ret = iommufd_spapr_remove_window(scontainer, info.tce32_start, errp);
    if (!ret) {
        error_setg_errno(errp, -ret,
                             "failed to remove existing window");
        return false;
    }

    if (vfio_host_win_del(scontainer, info.tce32_start,
                          info.tce32_start + info.tce32_size - 1) < 0) {
        error_setg(errp, "Failed to remove default window from tracking list");
        return false;
    }

    return true;
}

static void vfio_iommu_iommufd_spapr_class_init(ObjectClass *klass, const void *data)
{
    VFIOIOMMUClass *vioc = VFIO_IOMMU_CLASS(klass);

    vioc->add_window = iommufd_spapr_container_add_section_window;
    vioc->del_window = iommufd_spapr_container_del_section_window;
    vioc->release = iommufd_spapr_container_release;
    vioc->setup = vfio_spapr_iommufd_container_setup;
};

static const TypeInfo types[] = {
    {
        .name = TYPE_VFIO_IOMMU_SPAPR_IOMMUFD,
        .parent = TYPE_VFIO_IOMMU_IOMMUFD,
        .instance_size = sizeof(VFIOIOMMUFDSpaprContainer),
        .class_init = vfio_iommu_iommufd_spapr_class_init,
    },
};

DEFINE_TYPES(types)
