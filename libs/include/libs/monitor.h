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
 * @file monitor.h
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief Status monitor daemon implementation
 */

#ifndef _MONITOR_H
# define _MONITOR_H

int daemon_monitor_start(const char *dev_name,
			 int refresh,
			 int monitor_priority,
			 int monitor_time_slice);
int daemon_monitor_state(void);
int daemon_monitor_pause(void);
int daemon_monitor_resume(void);
int daemon_monitor_stop(void);

typedef enum				monitor_status {
	MONITOR_STOPPED = 1,
	MONITOR_RUNNING = 2,
	MONITOR_PAUSED = 3,
}					monitor_status;

#endif /* !_MONITOR_H */

