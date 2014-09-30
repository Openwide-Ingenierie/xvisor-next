/*
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
 * All rights reserved.
 * Code from the Linux kernel 3.16.
 * Modified by Jimmy Durand Wesolowski <jimmy.durand-wesolowski@openwide.fr>
 * for Xvisor.
 *
 * The code contained herein is licensed under the GNU Lesser General
 * Public License.  You may obtain a copy of the GNU Lesser General
 * Public License Version 2.1 or later at the following locations:
 *
 * http://www.opensource.org/licenses/lgpl-license.html
 * http://www.gnu.org/copyleft/lgpl.html
 *
 * @file reset.h
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief This file contains the reset driver support API.
 */

#ifndef _RESET_H_
#define _RESET_H_

struct vmm_device;
struct reset_control;

int reset_control_reset(struct reset_control *rstc);
int reset_control_assert(struct reset_control *rstc);
int reset_control_deassert(struct reset_control *rstc);

struct reset_control *reset_control_get(struct vmm_device *dev,
					const char *id);
void reset_control_put(struct reset_control *rstc);
struct reset_control *devm_reset_control_get(struct vmm_device *dev,
					     const char *id);

int device_reset(struct vmm_device *dev);

#endif /* _RESET_H_ */
