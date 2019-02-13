/*
 * QEMU PowerPC PAPR SCM backend definitions
 *
 * Copyright (c) 2020, IBM Corporation.
 *
 * This code is licensed under the GPL version 2 or later. See the
 * COPYING file in the top-level directory.
 */

#ifndef HW_SPAPR_NVDIMM_H
#define HW_SPAPR_NVDIMM_H

#include "hw/mem/nvdimm.h"
#include "hw/ppc/spapr.h"


void spapr_add_nvdimm(DeviceState *dev, uint64_t slot, Error **errp);
int spapr_pmem_dt_populate(SpaprDrc *drc, SpaprMachineState *spapr,
                           void *fdt, int *fdt_start_offset, Error **errp);

void spapr_create_nvdimm_dr_connectors(SpaprMachineState *spapr);

int spapr_dt_nvdimm(void *fdt, int parent_offset, NVDIMMDevice *nvdimm);

void spapr_dt_persistent_memory(void *fdt);

#endif
