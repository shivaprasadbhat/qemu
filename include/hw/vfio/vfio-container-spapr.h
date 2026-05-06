
/*
 * SPAPR VFIO container
 *
 * Copyright IBM, Corp. 2026
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_VFIO_CONTAINER_SPAPR_H_
#define HW_VFIO_CONTAINER_SPAPR_H_

#include "hw/vfio/vfio-container-legacy.h"

struct VFIOSpaprContainer {
    VFIOLegacyContainer parent_obj;

    MemoryListener prereg_listener;
    QLIST_HEAD(, VFIOHostDMAWindow) hostwin_list;
    unsigned int levels;
    unsigned int flags;
    unsigned int max_dynamic_windows_supported;
};

OBJECT_DECLARE_SIMPLE_TYPE(VFIOSpaprContainer, VFIO_IOMMU_SPAPR);

#endif /* HW_VFIO_CONTAINER_SPAPR_H */
