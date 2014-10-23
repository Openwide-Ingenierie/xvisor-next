/**
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
 * All rights reserved.
 * Modified by Jimmy Durand Wesolowski <jimmy.durand-wesolowski@openwide.fr>
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
 * @file cmd_monitor.c
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief Status monitor daemon command implementation
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_devtree.h>
#include <vmm_manager.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>
#include <libs/monitor.h>

#define MODULE_DESC			"Command monitor"
#define MODULE_AUTHOR			"Jimmy Durand Wesolowski"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_monitor_init
#define	MODULE_EXIT			cmd_monitor_exit

static void cmd_monitor_usage(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "Usage:\n");
	vmm_cprintf(cdev, "   monitor help\n");
	vmm_cprintf(cdev, "   monitor start [<device>] [<refresh>] "
			    "[<priority>] [<time slice>]\n");
	vmm_cprintf(cdev, "   monitor pause\n");
	vmm_cprintf(cdev, "   monitor resume\n");
	vmm_cprintf(cdev, "   monitor stop\n");
	vmm_cprintf(cdev, "   monitor state\n");
}

static int cmd_monitor_help(struct vmm_chardev *cdev)
{
	cmd_monitor_usage(cdev);
	return VMM_OK;
}

static int cmd_monitor_start(struct vmm_chardev *cdev, int argc, char *argv[])
{
	int refresh = -1;
	int priority = -1;
	int time_slice = -1;

	if (argc < 1) {
		cmd_monitor_usage(cdev);
		return VMM_EFAIL;
	}

	if (argc >= 2) {
		refresh = strtol(argv[1], NULL, 10);
	}

	if (argc >= 3) {
		priority = strtol(argv[2], NULL, 10);
	}

	if (argc >= 4) {
		time_slice = strtol(argv[3], NULL, 10);
	}

	return daemon_monitor_start(argv[0], refresh, priority, time_slice);
}

static int cmd_monitor_stop(struct vmm_chardev *cdev)
{
	return daemon_monitor_stop();
}

static int cmd_monitor_pause(struct vmm_chardev *cdev)
{
	return daemon_monitor_pause();
}

static int cmd_monitor_resume(struct vmm_chardev *cdev)
{
	return daemon_monitor_resume();
}

static int cmd_monitor_state(struct vmm_chardev *cdev)
{
	monitor_status state = MONITOR_RUNNING;

	state = daemon_monitor_state();

	switch (state)
	{
	case MONITOR_STOPPED:
		vmm_cprintf(cdev, "Monitor stopped\n");
		break;
	case MONITOR_RUNNING:
		vmm_cprintf(cdev, "Monitor running\n");
		break;
	case MONITOR_PAUSED:
		vmm_cprintf(cdev, "Monitor paused\n");
		break;
	default:
		vmm_cprintf(cdev, "Unknown monitor state\n");
		return VMM_EFAIL;
	}

	return VMM_OK;
}

static int cmd_monitor_exec(struct vmm_chardev *cdev, int argc, char **argv)
{
	if (argc <= 1) {
		cmd_monitor_usage(cdev);
		return VMM_EFAIL;
	}

	if (!strcmp(argv[1], "help")) {
		return cmd_monitor_help(cdev);
	} else if (!strcmp(argv[1], "start")) {
		return cmd_monitor_start(cdev, argc - 2, argv + 2);
	} else if (!strcmp(argv[1], "stop")) {
		return cmd_monitor_stop(cdev);
	} else if (!strcmp(argv[1], "pause")) {
		return cmd_monitor_pause(cdev);
	} else if (!strcmp(argv[1], "resume")) {
		return cmd_monitor_resume(cdev);
	} else if (!strcmp(argv[1], "state")) {
		return cmd_monitor_state(cdev);
	} else {
		cmd_monitor_usage(cdev);
	}

	return VMM_EFAIL;
}

static struct vmm_cmd cmd_monitor = {
	.name = "monitor",
	.desc = "control commands for vcpu",
	.usage = cmd_monitor_usage,
	.exec = cmd_monitor_exec,
};

static int __init cmd_monitor_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_monitor);
}

static void __exit cmd_monitor_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_monitor);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
