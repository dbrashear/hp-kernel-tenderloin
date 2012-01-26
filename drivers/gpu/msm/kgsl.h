/* Copyright (c) 2008-2010, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of Code Aurora Forum, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
#ifndef _GSL_DRIVER_H
#define _GSL_DRIVER_H

#include <linux/types.h>
#include <linux/msm_kgsl.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/regulator/consumer.h>

#include <asm/atomic.h>

#include "kgsl_device.h"
#include "kgsl_sharedmem.h"

#define DRIVER_NAME "kgsl"
#define CLASS_NAME "msm_kgsl"
#define CHIP_REV_251 0x020501

/* Flags to control whether to flush or invalidate a cached memory range */
#define KGSL_CACHE_INV		0x00000000
#define KGSL_CACHE_CLEAN	0x00000001
#define KGSL_CACHE_FLUSH	0x00000002

#define KGSL_CACHE_USER_ADDR	0x00000010
#define KGSL_CACHE_VMALLOC_ADDR	0x00000020

/*cache coherency ops */
#define DRM_KGSL_GEM_CACHE_OP_TO_DEV	0x0001
#define DRM_KGSL_GEM_CACHE_OP_FROM_DEV	0x0002

#define KGSL_NUM_2D_DEVICES 2
#define IDX_2D(X) ((X)-KGSL_DEVICE_2D0)

struct kgsl_driver {
	struct cdev cdev;
	dev_t dev_num;
	struct class *class;
	struct kgsl_device *devp[KGSL_DEVICE_MAX];
	int num_devs;
	struct platform_device *pdev;

	uint32_t flags_debug;

	struct kgsl_sharedmem shmem;

	/* Global lilst of open processes */
	struct list_head process_list;
	/* Global list of pagetables */
	struct list_head pagetable_list;
	/* Mutex for accessing the pagetable list */
	struct mutex pt_mutex;
	/* Mutex for accessing the process list */
	struct mutex process_mutex;

	struct kgsl_pagetable *global_pt;
};

extern struct kgsl_driver kgsl_driver;

struct kgsl_mem_entry {
	struct kgsl_memdesc memdesc;
	struct file *file_ptr;
	struct list_head list;
	uint32_t free_timestamp;
	/* back pointer to private structure under whose context this
	* allocation is made */
	struct kgsl_process_private *priv;
};

enum kgsl_status {
	KGSL_SUCCESS = 0,
	KGSL_FAILURE = 1
};

#define KGSL_TRUE 1
#define KGSL_FALSE 0

#ifdef CONFIG_MSM_KGSL_MMU_PAGE_FAULT
#define MMU_CONFIG 2
#else
#define MMU_CONFIG 1
#endif

void kgsl_destroy_mem_entry(struct kgsl_mem_entry *entry);
uint8_t *kgsl_sharedmem_convertaddr(struct kgsl_device *device,
	unsigned int pt_base, unsigned int gpuaddr, unsigned int *size);
int kgsl_idle(struct kgsl_device *device, unsigned int timeout);
int kgsl_setstate(struct kgsl_device *device, uint32_t flags);
int kgsl_regread(struct kgsl_device *device, unsigned int offsetwords,
			unsigned int *value);
int kgsl_regwrite(struct kgsl_device *device, unsigned int offsetwords,
			unsigned int value);
int kgsl_check_timestamp(struct kgsl_device *device, unsigned int timestamp);

int kgsl_setup_pt(struct kgsl_pagetable *);

int kgsl_cleanup_pt(struct kgsl_pagetable *);

int kgsl_register_ts_notifier(struct kgsl_device *device,
			      struct notifier_block *nb);

int kgsl_unregister_ts_notifier(struct kgsl_device *device,
				struct notifier_block *nb);

#ifdef CONFIG_MSM_KGSL_DRM
extern int kgsl_drm_init(struct platform_device *dev);
extern void kgsl_drm_exit(void);
extern void kgsl_gpu_mem_flush(int op);
#else
static inline int kgsl_drm_init(struct platform_device *dev)
{
	return 0;
}

static inline void kgsl_drm_exit(void)
{
}
#endif

static inline int kgsl_gpuaddr_in_memdesc(const struct kgsl_memdesc *memdesc,
				unsigned int gpuaddr)
{
	if (gpuaddr >= memdesc->gpuaddr && (gpuaddr + sizeof(unsigned int)) <=
		(memdesc->gpuaddr + memdesc->size)) {
		return 1;
	}
	return 0;
}

#endif /* _GSL_DRIVER_H */
