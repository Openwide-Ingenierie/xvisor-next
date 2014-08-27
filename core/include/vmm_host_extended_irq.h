/**
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
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
 * @file vmm_host_extended_irq.h
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief Extended IRQ support, kind of IRQ domain for Xvisor.
 */

#ifndef _VMM_HOST_EXTENDED_IRQ_H__
# define _VMM_HOST_EXTENDED_IRQ_H__

# ifdef CONFIG_EXTENDED_IRQ

#include <vmm_types.h>
#include <vmm_error.h>
#include <vmm_host_irq.h>

struct vmm_host_extirqs {
	u32 base;
	u32 count;
	u32 hwirq;
	void *dev;
	struct vmm_host_irq *irqs;
};

struct vmm_host_irq *vmm_host_extirq_get(u32 eirq_no);

u32 vmm_host_extirq_get_irq(struct vmm_host_extirqs *group,
			    unsigned offset);

u32 vmm_host_extirqs_get_offset(struct vmm_host_extirqs *group,
				u32 irq);

int vmm_host_extirq_map(u32 hwirq,
			const char *name,
			u32 size,
			struct vmm_host_irq_chip *chip,
			void *chip_data,
			void *dev,
			struct vmm_host_extirqs **extirqs);

int vmm_host_extirq_init(void);

# else /* !CONFIG_EXTENDED_IRQ */
#  define vmm_host_extirq_get(IRQ_NO)			VMM_ENOTAVAIL

#  define vmm_host_extirq_get_irq(GROUP, OFFSET)	VMM_ENOTAVAIL

#  define vmm_host_extirqs_get_offset(GROUP, IRQ)	VMM_ENOTAVAIL

static inline int vmm_host_extirq_map(u32 hirq_num,
				      const char *name,
				      u32 size,
				      void *dev,
				      struct vmm_host_extirqs **extirqs)
{
	return VMM_ENOTAVAIL;
}

static inline int vmm_host_extirq_init(void)
{
	return VMM_OK;
}

# endif /* !CONFIG_EXTENDED_IRQ */

#endif /* _VMM_HOST_EXTENDED_IRQ_H__ */
