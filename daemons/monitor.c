/**
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
 * All rights reserved.
 * Modified by Jimmy Durand Wesolowski <jimmy.durand-wesolowski@openwide.fr>
 * Inspired from mterm.c from Anup Patel Copyright (c) 2012.
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
 * @file monitor.c
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief Status monitor daemon implementation
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_heap.h>
#include <vmm_main.h>
#include <vmm_delay.h>
#include <vmm_devtree.h>
#include <vmm_threads.h>
#include <vmm_modules.h>
#include <vmm_chardev.h>
#include <vmm_manager.h>
#include <vmm_scheduler.h>
#include <vmm_host_aspace.h>
#include <vmm_host_ram.h>
#include <libs/mathlib.h>
#include <libs/monitor.h>

#define MODULE_NAME			"monitor"

static struct monitor_ctrl {
	struct vmm_thread *thread;
	u32 msec_sleep;
	enum monitor_status status;
} monctrl = {
	.status = MONITOR_STOPPED,
};

static int vcpu_list_iter(struct vmm_vcpu *vcpu, void *priv)
{
	u32 hcpu;
	char state[10];
	struct vmm_chardev *cdev = priv;

	switch (vmm_manager_vcpu_get_state(vcpu)) {
	case VMM_VCPU_STATE_UNKNOWN:
		strcpy(state, "Unknown  ");
		break;
	case VMM_VCPU_STATE_RESET:
		strcpy(state, "Reset    ");
		break;
	case VMM_VCPU_STATE_READY:
		strcpy(state, "Ready    ");
		break;
	case VMM_VCPU_STATE_RUNNING:
		strcpy(state, "Running  ");
		break;
	case VMM_VCPU_STATE_PAUSED:
		strcpy(state, "Paused   ");
		break;
	case VMM_VCPU_STATE_HALTED:
		strcpy(state, "Halted   ");
		break;
	default:
		strcpy(state, "Invalid  ");
		break;
	}
	vmm_cprintf(cdev, " %-3d", vcpu->id);
#ifdef CONFIG_SMP
	vmm_manager_vcpu_get_hcpu(vcpu, &hcpu);
	vmm_cprintf(cdev, " %-3d", hcpu);
#endif
	vmm_cprintf(cdev, " %-17s %-10s\n", vcpu->name, state);

	return VMM_OK;
}

static int monitor_main(void *priv)
{
	u32 cpu = 0;
	u32 util = 0;
	u32 frame = 0;
	struct vmm_chardev *cdev = priv;

	if (NULL == priv)
		return VMM_EFAIL;

	while (1) {
		if (cdev->ioctl)
			cdev->ioctl(cdev, VMM_CHARDEV_RESET, NULL);

		for_each_online_cpu(cpu) {
			util = udiv64(vmm_scheduler_idle_time(cpu) * 1000,
				      vmm_scheduler_idle_time_get_period(cpu));
			util = (util > 1000) ? 1000 : util;
			util = 1000 - util;
			vmm_cprintf(cdev, "CPU%d: %d.%d %%   \n",
				    cpu, udiv32(util, 10), umod32(util, 10));
		}
		frame = vmm_host_ram_total_frame_count();
		frame -= vmm_host_ram_free_frame_count();
		frame /= 1024;
		frame *= VMM_PAGE_SIZE;
		vmm_cprintf(cdev, "Xvisor RAM usage: %d kB\n", frame);

		vmm_manager_vcpu_iterate(vcpu_list_iter, cdev);
		vmm_msleep(monctrl.msec_sleep);
	}

	return VMM_OK;
}

int daemon_monitor_start(const char *dev_name,
			 int refresh,
			 int monitor_priority,
			 int monitor_time_slice)
{
	struct vmm_chardev* cdev = NULL;

	/* Reset the control structure */
	memset(&monctrl, 0, sizeof(monctrl));

	if (-1 == monitor_priority) {
		monitor_priority = VMM_THREAD_DEF_PRIORITY;
	}
	if (-1 == monitor_time_slice) {
		monitor_time_slice = VMM_THREAD_DEF_TIME_SLICE;
	}
	if (1000 < refresh) {
		vmm_printf(MODULE_NAME " error: Refresh rate is too high");
		return VMM_EFAIL;
	}
	else if (-1 == refresh) {
		monctrl.msec_sleep = 500;
	} else {
		monctrl.msec_sleep = udiv32(1000, refresh);
	}

	cdev = vmm_chardev_find(dev_name);
	if (NULL == cdev) {
		vmm_printf(MODULE_NAME " error: Failed to open \"%s\"\n",
			   dev_name);
		return VMM_EFAIL;
	}

	/* Create monitor thread */
	monctrl.thread = vmm_threads_create("monitor",
					   &monitor_main,
					   cdev,
					   monitor_priority,
					   monitor_time_slice);
	if (!monctrl.thread) {
		vmm_panic("Creation of system critical thread failed.\n");
	}

	/* Start the monitor thread */
	vmm_threads_start(monctrl.thread);
	monctrl.status = MONITOR_RUNNING;

	return VMM_OK;
}

int daemon_monitor_state(void)
{
	return monctrl.status;
}

int daemon_monitor_pause(void)
{
	int rc = VMM_OK;

	if (MONITOR_RUNNING != monctrl.status) {
		vmm_printf(MODULE_NAME " error: monitor is not running\n");
		return VMM_EFAIL;
	}

	rc = vmm_threads_sleep(monctrl.thread);
	if (VMM_OK == rc)
		monctrl.status = MONITOR_PAUSED;

	return rc;
}

int daemon_monitor_resume(void)
{
	int rc = VMM_OK;

	if (MONITOR_PAUSED != monctrl.status) {
		vmm_printf(MODULE_NAME " error: monitor is not paused\n");
		return VMM_EFAIL;
	}

	rc = vmm_threads_wakeup(monctrl.thread);
	if (VMM_OK == rc)
		monctrl.status = MONITOR_RUNNING;

	return rc;
}

int daemon_monitor_stop(void)
{
	int rc = VMM_OK;

	if (MONITOR_STOPPED == monctrl.status)
		return VMM_EFAIL;

	rc = vmm_threads_stop(monctrl.thread);

	if (VMM_OK == rc)
		return vmm_threads_destroy(monctrl.thread);
	return rc;
}
