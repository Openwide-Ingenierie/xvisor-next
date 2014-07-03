/**
 * Copyright (c) 2010 Anup Patel.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * @file vmm_main.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief main source file to start, stop and reset hypervisor
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_devtree.h>
#include <vmm_stdio.h>
#include <vmm_version.h>
#include <vmm_host_aspace.h>
#include <vmm_host_irq.h>
#include <vmm_smp.h>
#include <vmm_percpu.h>
#include <vmm_clocksource.h>
#include <vmm_clockchip.h>
#include <vmm_timer.h>
#include <vmm_delay.h>
#include <vmm_manager.h>
#include <vmm_scheduler.h>
#include <vmm_loadbal.h>
#include <vmm_threads.h>
#include <vmm_profiler.h>
#include <vmm_devdrv.h>
#include <vmm_devemu.h>
#include <vmm_workqueue.h>
#include <vmm_cmdmgr.h>
#include <vmm_wallclock.h>
#include <vmm_chardev.h>
#include <vmm_modules.h>
#include <arch_cpu.h>
#include <arch_board.h>

/* Optional includes */
#include <drv/rtc.h>

void __noreturn vmm_hang(void)
{
	while (1) ;
}

static struct vmm_work sys_init;
static struct vmm_work sys_postinit;
static bool sys_init_done = FALSE;

bool vmm_init_done(void)
{
	return sys_init_done;
}

static void system_postinit_work(struct vmm_work *work)
{
#define BOOTCMD_WIDTH		256
	char bcmd[BOOTCMD_WIDTH];
	const char *str;
	u32 c, freed;
	struct vmm_chardev *cdev;
#if defined(CONFIG_RTC)
	int ret;
	struct rtc_device *rdev;
#endif
	struct vmm_devtree_node *node, *node1;

	/* Print status of present host CPUs */
	for_each_present_cpu(c) {
		if (vmm_cpu_online(c)) {
			vmm_printf("CPU%d: Online\n", c);
		} else {
			vmm_printf("CPU%d: Possible\n", c);
		}
	}
	vmm_printf("Brought Up %d CPUs\n", vmm_num_online_cpus());

	/* Free init memory */
	vmm_printf("Freeing init memory: ");
	freed = vmm_host_free_initmem();
	vmm_printf("%dK\n", freed);

	/* Process attributes in chosen node */
	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
				   VMM_DEVTREE_CHOSEN_NODE_NAME);
	if (node) {
		/* Find character device based on console attribute */
		str = NULL;
		vmm_devtree_read_string(node,
					VMM_DEVTREE_CONSOLE_ATTR_NAME, &str);
		if (!(cdev = vmm_chardev_find(str))) {
			if ((node1 = vmm_devtree_getnode(str))) {
				cdev = vmm_chardev_find(node1->name);
			}
		}
		/* Set chosen console device as stdio device */
		if (cdev) {
			vmm_printf("Change stdio device to %s\n", cdev->name);
			vmm_stdio_change_device(cdev);
		}

#if defined(CONFIG_RTC)
		/* Find rtc device based on rtc_device attribute */
		str = NULL;
		vmm_devtree_read_string(node,
					VMM_DEVTREE_RTCDEV_ATTR_NAME, &str);
		if (!(rdev = rtc_device_find(str))) {
			if ((node1 = vmm_devtree_getnode(str))) {
				rdev = rtc_device_find(node1->name);
			}
		}
		/* Syncup wallclock time with chosen rtc device */
		if (rdev) {
			ret = rtc_device_sync_wallclock(rdev);
			vmm_printf("Syncup wallclock using %s", rdev->name);
			if (ret) {
				vmm_printf("(error %d)", ret);
			}
			vmm_printf("\n");
		}
#endif

		/* Execute boot commands */
		if (vmm_devtree_read_string(node,
			VMM_DEVTREE_BOOTCMD_ATTR_NAME, &str) == VMM_OK) {
			c = vmm_devtree_attrlen(node,
						VMM_DEVTREE_BOOTCMD_ATTR_NAME);
			while (c) {
				/* Print boot command */
				vmm_printf("bootcmd: %s\n", str);
				/* Execute boot command */
				strlcpy(bcmd, str, sizeof(bcmd));
				cdev = vmm_stdio_device();
				vmm_cmdmgr_execute_cmdstr(cdev, bcmd, NULL);
				/* Next boot command */
				c -= strlen(str) + 1;
				str += strlen(str) + 1;
			}
		}
	}

	/* Set system init done flag */
	sys_init_done = TRUE;
}

static void system_init_work(struct vmm_work *work)
{
	int ret;
#if defined(CONFIG_SMP)
	u32 c;
#endif

	/* Initialize wallclock */
	vmm_printf("Initialize Wallclock Subsystem\n");
	ret = vmm_wallclock_init();
	if (ret) {
		vmm_panic("Error %d\n", ret);
	}

#if defined(CONFIG_SMP)
	/* Initialize secondary CPUs */
	vmm_printf("Initialize Secondary CPUs\n");
	ret = arch_smp_init_cpus();
	if (ret) {
		vmm_panic("Error %d\n", ret);
	}

	/* Prepare secondary CPUs */
	ret = arch_smp_prepare_cpus(vmm_num_possible_cpus());
	if (ret) {
		vmm_panic("Error %d\n", ret);
	}

	/* Start each present secondary CPUs */
	for_each_present_cpu(c) {
		if (c == vmm_smp_bootcpu_id()) {
			continue;
		}
		ret = arch_smp_start_cpu(c);
		if (ret) {
			vmm_printf("Failed to start CPU%d (error %d)\n",
				   c, ret);
		}
	}

	/* Initialize hypervisor load balancer */
	vmm_printf("Initialize Hypervisor Load Balancer\n");
	ret = vmm_loadbal_init();
	if (ret) {
		vmm_panic("Error %d\n", ret);
	}
#endif

	/* Initialize command manager */
	vmm_printf("Initialize Command Manager\n");
	ret = vmm_cmdmgr_init();
	if (ret) {
		vmm_panic("Error %d\n", ret);
	}

	/* Initialize device driver framework */
	vmm_printf("Initialize Device Driver Framework\n");
	ret = vmm_devdrv_init();
	if (ret) {
		vmm_panic("Error %d\n", ret);
	}

	/* Initialize device emulation framework */
	vmm_printf("Initialize Device Emulation Framework\n");
	ret = vmm_devemu_init();
	if (ret) {
		vmm_panic("Error %d\n", ret);
	}

	/* Initialize character device framework */
	vmm_printf("Initialize Character Device Framework\n");
	ret = vmm_chardev_init();
	if (ret) {
		vmm_panic("Error %d\n", ret);
	}

#if defined(CONFIG_SMP)
	/* Poll for all present CPUs to become online */
	/* Note: There is a timeout of 1 second */
	/* Note: The modules might use SMP IPIs or might have per-cpu context 
	 * so, we do this before vmm_modules_init() in-order to make sure that 
	 * correct number of online CPUs are visible to all modules.
	 */
	ret = 1000;
	while(ret--) {
		int all_cpu_online = 1;

		for_each_present_cpu(c) {
			if (!vmm_cpu_online(c)) {
				all_cpu_online = 0;
			}
		}

		if (all_cpu_online) {
			break;
		}

		vmm_mdelay(1);
	}
#endif

	/* Initialize hypervisor modules */
	vmm_printf("Initialize Hypervisor Modules\n");
	ret = vmm_modules_init();
	if (ret) {
		vmm_panic("Error %d\n", ret);
	}

	/* Initialize cpu final */
	vmm_printf("Initialize CPU Final\n");
	ret = arch_cpu_final_init();
	if (ret) {
		vmm_panic("Error %d\n", ret);
	}

	/* Intialize board final */
	vmm_printf("Initialize Board Final\n");
	ret = arch_board_final_init();
	if (ret) {
		vmm_panic("Error %d\n", ret);
	}

	/* Schedule system post-init work */
	INIT_WORK(&sys_postinit, &system_postinit_work);
	vmm_workqueue_schedule_work(NULL, &sys_postinit);
}

static void __init init_bootcpu(void)
{
	int ret;
	struct vmm_devtree_node *node;

	/* Sanity check on SMP processor id */
	if (CONFIG_CPU_COUNT <= vmm_smp_processor_id()) {
		vmm_hang();
	}

	/* Mark this CPU possible & present */
	vmm_set_cpu_possible(vmm_smp_processor_id(), TRUE);
	vmm_set_cpu_present(vmm_smp_processor_id(), TRUE);

	/* Print version string */
	vmm_printf("\n");
	vmm_printf("%s v%d.%d.%d (%s %s)\n", VMM_NAME, 
		   VMM_VERSION_MAJOR, VMM_VERSION_MINOR, VMM_VERSION_RELEASE,
		   __DATE__, __TIME__);
	vmm_printf("\n");

	/* Initialize host virtual address space */
	vmm_printf("Initialize Host Address Space\n");
	ret = vmm_host_aspace_init();
	if (ret) {
		goto init_bootcpu_fail;
	}

	/* Initialize heap */
	vmm_printf("Initialize Heap Management\n");
	ret = vmm_heap_init();
	if (ret) {
		goto init_bootcpu_fail;
	}

	/* Initialize per-cpu area */
	vmm_printf("Initialize PerCPU Areas\n");
	ret = vmm_percpu_init();
	if (ret) {
		goto init_bootcpu_fail;
	}

	/* Initialize device tree */
	vmm_printf("Initialize Device Tree\n");
	ret = vmm_devtree_init();
	if (ret) {
		goto init_bootcpu_fail;
	}

	/* Make sure /guests and /vmm nodes are present */
	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
				   VMM_DEVTREE_GUESTINFO_NODE_NAME);
	if (!node) {
		vmm_devtree_addnode(NULL, VMM_DEVTREE_GUESTINFO_NODE_NAME);
	}
	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
				   VMM_DEVTREE_VMMINFO_NODE_NAME);
	if (!node) {
		vmm_devtree_addnode(NULL, VMM_DEVTREE_VMMINFO_NODE_NAME);
	}

	/* Initialize host interrupts */
	vmm_printf("Initialize Host IRQ\n");
	ret = vmm_host_irq_init();
	if (ret) {
		goto init_bootcpu_fail;
	}

	/* Initialize CPU early */
	vmm_printf("Initialize CPU Early\n");
	ret = arch_cpu_early_init();
	if (ret) {
		goto init_bootcpu_fail;
	}

	/* Initialize Board early */
	vmm_printf("Initialize Board Early\n");
	ret = arch_board_early_init();
	if (ret) {
		goto init_bootcpu_fail;
	}

	/* Initialize standerd input/output */
	vmm_printf("Initialize Standard I/O\n");
	ret = vmm_stdio_init();
	if (ret) {
		goto init_bootcpu_fail;
	}

	/* Initialize clocksource manager */
	vmm_printf("Initialize Clocksource Manager\n");
	ret = vmm_clocksource_init();
	if (ret) {
		goto init_bootcpu_fail;
	}

	/* Initialize clockchip manager */
	vmm_printf("Initialize Clockchip Manager\n");
	ret = vmm_clockchip_init();
	if (ret) {
		goto init_bootcpu_fail;
	}

	/* Initialize hypervisor timer */
	vmm_printf("Initialize Hypervisor Timer\n");
	ret = vmm_timer_init();
	if (ret) {
		goto init_bootcpu_fail;
	}

	/* Initialize soft delay */
	vmm_printf("Initialize Soft Delay\n");
	ret = vmm_delay_init();
	if (ret) {
		goto init_bootcpu_fail;
	}

	/* Initialize hypervisor manager */
	vmm_printf("Initialize Hypervisor Manager\n");
	ret = vmm_manager_init();
	if (ret) {
		goto init_bootcpu_fail;
	}

	/* Initialize hypervisor scheduler */
	vmm_printf("Initialize Hypervisor Scheduler\n");
	ret = vmm_scheduler_init();
	if (ret) {
		goto init_bootcpu_fail;
	}

	/* Initialize hypervisor threads */
	vmm_printf("Initialize Hypervisor Threads\n");
	ret = vmm_threads_init();
	if (ret) {
		goto init_bootcpu_fail;
	}

#ifdef CONFIG_PROFILE
	/* Intialize hypervisor profiler */
	vmm_printf("Initialize Hypervisor Profiler\n");
	ret = vmm_profiler_init();
	if (ret) {
		goto init_bootcpu_fail;
	}
#endif

#if defined(CONFIG_SMP)
	/* Initialize inter-processor interrupts */
	vmm_printf("Initialize Inter Processor Interrupts\n");
	ret = vmm_smp_ipi_init();
	if (ret) {
		goto init_bootcpu_fail;
	}
#endif

	/* Initialize workqueue framework */
	vmm_printf("Initialize Workqueue Framework\n");
	ret = vmm_workqueue_init();
	if (ret) {
		goto init_bootcpu_fail;
	}

	/* Schedule system init work */
	INIT_WORK(&sys_init, &system_init_work);
	vmm_workqueue_schedule_work(NULL, &sys_init);

	/* Start timer (Must be last step) */
	vmm_timer_start();

	/* Wait here till scheduler gets invoked by timer */
	vmm_hang();

init_bootcpu_fail:
	vmm_printf("Error %d\n", ret);
	vmm_hang();
}

#if defined(CONFIG_SMP)
static void __cpuinit init_secondary(void)
{
	int ret;

	/* Sanity check on SMP processor ID */
	if (CONFIG_CPU_COUNT <= vmm_smp_processor_id()) {
		vmm_hang();
	}

	/* This function should not be called by Boot CPU */
	if (vmm_smp_is_bootcpu()) {
		vmm_hang();
	}

	/* Initialize host virtual address space */
	ret = vmm_host_aspace_init();
	if (ret) {
		vmm_hang();
	}

	/* Initialize host interrupts */
	ret = vmm_host_irq_init();
	if (ret) {
		vmm_hang();
	}

	/* Initialize clockchip manager */
	ret = vmm_clockchip_init();
	if (ret) {
		vmm_hang();
	}

	/* Initialize hypervisor timer */
	ret = vmm_timer_init();
	if (ret) {
		vmm_hang();
	}

	/* Initialize soft delay */
	ret = vmm_delay_init();
	if (ret) {
		vmm_hang();
	}

	/* Initialize hypervisor scheduler */
	ret = vmm_scheduler_init();
	if (ret) {
		vmm_hang();
	}

	/* Initialize inter-processor interrupts */
	ret = vmm_smp_ipi_init();
	if (ret) {
		vmm_hang();
	}

	/* Initialize workqueue framework */
	ret = vmm_workqueue_init();
	if (ret) {
		vmm_hang();
	}

	/* Inform architecture code about secondary cpu */
	arch_smp_postboot();

	/* Start timer (Must be last step) */
	vmm_timer_start();

	/* Wait here till scheduler gets invoked by timer */
	vmm_hang();
}
#endif

void __cpuinit vmm_init(void)
{
#if defined(CONFIG_SMP)
	/* Mark this CPU as Boot CPU
	 * Note: This will only work on first CPU.
	 */
	vmm_smp_set_bootcpu();

	if (vmm_smp_is_bootcpu()) { /* Boot CPU */
		init_bootcpu();
	} else { /* Secondary CPUs */
		init_secondary();
	}
#else
	/* Boot CPU */
	init_bootcpu();
#endif
}

static void system_stop(void)
{
	/* Stop scheduler */
	vmm_printf("Stopping Hypervisor Timer\n");
	vmm_timer_stop();

	/* FIXME: Do other cleanup stuff. */
}

static int (*system_reset)() = NULL;

void vmm_register_system_reset(int (*callback)())
{
	system_reset = callback;
}

void vmm_reset(void)
{
	int rc;

	/* Stop the system */
	system_stop();

	/* Issue system reset */
	if (!system_reset) {
		vmm_printf("Error: no system reset callback.\n");
		vmm_printf("Please reset system manually ...\n");
	} else {
		vmm_printf("Issuing System Reset\n");
		if ((rc = system_reset())) {
			vmm_printf("Error: reset failed (error %d)\n", rc);
		}
	}

	/* Wait here. Nothing else to do. */
	vmm_hang();
}

static int (*system_shutdown)() = NULL;

void vmm_register_system_shutdown(int (*callback)())
{
	system_shutdown = callback;
}

void vmm_shutdown(void)
{
	int rc;

	/* Stop the system */
	system_stop();

	/* Issue system shutdown */
	if (!system_shutdown) {
		vmm_printf("Error: no system shutdown callback.\n");
		vmm_printf("Please shutdown system manually ...\n");
	} else {
		vmm_printf("Issuing System Shutdown\n");
		if ((rc = system_shutdown())) {
			vmm_printf("Error: shutdown failed (error %d)\n", rc);
		}
	}

	/* Wait here. Nothing else to do. */
	vmm_hang();
}
