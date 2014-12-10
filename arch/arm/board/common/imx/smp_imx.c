/*
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
 * All rights reserved.
 *
 * This file is part of Xvisor.
 *
 * Xvisor is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * Xvisor is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Xvisor.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @file smp_imx.c
 * @author Julien Viard de Galbert (julien.viarddegalbert@openwide.fr)
 * @brief Freescale i.MX6 secific SMP operations
 *
 * Adapted from Linux Kernel 3.18 arch/arm/mach-imx/platsmp.c
 *
 * Copyright 2011 Freescale Semiconductor, Inc.
 * Copyright 2011 Linaro Ltd.
 * The original source is licensed under GPL.
 */

#include <vmm_error.h>
#include <vmm_smp.h>
#include <vmm_cache.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_host_aspace.h>
#include <vmm_stdio.h>


#include <imx-common.h>
#include <smp_ops.h>

#undef DEBUG
#define DEBUG

#ifdef DEBUG
#define DPRINTF(msg...)		vmm_printf(msg)
#else
#define DPRINTF(msg...)
#endif

/* @todo: most of this is copied from smp_scu.c, factorize it !! */

#define SCU_PM_NORMAL	0
#define SCU_PM_EINVAL	1
#define SCU_PM_DORMANT	2
#define SCU_PM_POWEROFF	3

#define SCU_CTRL		0x00
#define SCU_CONFIG		0x04
#define SCU_CPU_STATUS		0x08
#define SCU_INVALIDATE		0x0c
#define SCU_FPGA_REVISION	0x10

#ifdef CONFIG_SMP
/*
 * Get the number of CPU cores from the SCU configuration
 */
static u32 scu_get_core_count(void *scu_base)
{
	return (vmm_readl(scu_base + SCU_CONFIG) & 0x03) + 1;
}

/*
 * Enable the SCU
 */
static void scu_enable(void *scu_base)
{
	u32 scu_ctrl;

#ifdef CONFIG_ARM_ERRATA_764369
	/*
	 * This code is mostly for TEGRA 2 and 3 processors. 
	 * This in not enabled or tested on Xvisor for now.
	 * We keep it as we might have to enable it someday.
	 */
	/* Cortex-A9 only */
	if ((read_cpuid(CPUID_ID) & 0xff0ffff0) == 0x410fc090) {

		scu_ctrl = vmm_readl(scu_base + 0x30);
		if (!(scu_ctrl & 1)) {
			vmm_writel(scu_ctrl | 0x1, scu_base + 0x30);
		}
	}
#endif

	scu_ctrl = vmm_readl(scu_base + SCU_CTRL);
	/* already enabled? */
	if (scu_ctrl & 1) {
		return;
	}

	scu_ctrl |= 1;
	vmm_writel(scu_ctrl, scu_base + SCU_CTRL);

	/*
	 * Ensure that the data accessed by CPU0 before the SCU was
	 * initialised is visible to the other CPUs.
	 */
	vmm_flush_cache_all();
}
#endif

#if defined(CONFIG_ARM_SMP_OPS) && defined(CONFIG_ARM_GIC)

static virtual_addr_t scu_base;

static struct vmm_devtree_nodeid scu_matches[] = {
	{.compatible = "arm,arm11mp-scu"},
	{.compatible = "arm,cortex-a9-scu"},
	{ /* end of list */ },
};

static int __init imx_cpu_init(struct vmm_devtree_node *node,
				unsigned int cpu)
{
	int rc;
	u32 ncores;
	struct vmm_devtree_node *scu_node;

	/* Map SCU base */
	if (!scu_base) {
		scu_node = vmm_devtree_find_matching(NULL, scu_matches);
		if (!scu_node) {
			return VMM_ENODEV;
		}
		rc = vmm_devtree_regmap(scu_node, &scu_base, 0);
		if (rc) {
			return rc;
		}
	}

	/* Check core count from SCU */
	ncores = scu_get_core_count((void *)scu_base);
	if (ncores <= cpu) {
		return VMM_ENOSYS;
	}

	return VMM_OK;
}

static int __init imx_cpu_prepare(unsigned int cpu)
{
	/* Enable snooping through SCU */
	if (scu_base) {
		scu_enable((void *)scu_base);
	}

	return VMM_OK;
}


extern u8 _start_secondary_nopen;

static int __init imx_cpu_boot(unsigned int cpu)
{
	imx_set_cpu_jump(cpu, (void*)&_start_secondary_nopen);
	imx_enable_cpu(cpu, true);
	return VMM_OK;
}

static struct smp_operations smp_imx_ops = {
	.name = "smp-imx",
	.cpu_init = imx_cpu_init,
	.cpu_prepare = imx_cpu_prepare,
	.cpu_boot = imx_cpu_boot,
};

SMP_OPS_DECLARE(smp_imx, &smp_imx_ops);

#endif
