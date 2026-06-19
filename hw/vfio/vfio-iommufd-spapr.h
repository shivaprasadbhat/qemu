/*
 * VFIO iommufd
 *
 * Copyright Red Hat, Inc. 2025
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_VFIO_VFIO_SPAPR_IOMMUFD_H
#define HW_VFIO_VFIO_SPAPR_IOMMUFD_H

#include "hw/vfio/vfio-iommufd.h"

typedef struct VFIOHostDMAWindow {
    hwaddr min_iova;
    hwaddr max_iova;
    uint64_t iova_pgsizes;
    QLIST_ENTRY(VFIOHostDMAWindow) hostwin_next;
} VFIOHostDMAWindow;


struct VFIOIOMMUFDSpaprContainer {
    VFIOIOMMUFDContainer parent_obj;

    QLIST_HEAD(, VFIOHostDMAWindow) hostwin_list;
    uint64_t tce32_start;
    uint64_t tce32_size;
    __u64 pgsizes;
    unsigned int levels;
    unsigned int flags;
    unsigned int max_dynamic_windows_supported;
};

OBJECT_DECLARE_SIMPLE_TYPE(VFIOIOMMUFDSpaprContainer, VFIO_IOMMU_SPAPR_IOMMUFD);

#endif /* HW_VFIO_VFIO_SPAPR_IOMMUFD_H */
