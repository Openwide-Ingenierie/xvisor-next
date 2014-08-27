/**
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * @file vmm_host_extended_irq.c
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief Extended IRQ support, kind of IRQ domain for Xvisor.
 */

#include <vmm_host_extended_irq.h>
#include <vmm_spinlocks.h>
#include <vmm_stdio.h>
#include <vmm_heap.h>

struct vmm_host_extirqs_ctrl {
	vmm_spinlock_t lock;
	u32 count;
	u32 base;
	struct vmm_host_irq **irqs;
	struct vmm_host_extirqs groups[CONFIG_EXTENDED_IRQ_GROUP_NB];
};

static struct vmm_host_extirqs_ctrl extirqctrl;

struct vmm_host_irq *vmm_host_extirq_get(u32 eirq_num)
{
	u32 irq_num = eirq_num - CONFIG_HOST_IRQ_COUNT;

	if (irq_num > extirqctrl.base)
		return NULL;

	return extirqctrl.irqs[irq_num];
}

u32 vmm_host_extirq_get_irq(struct vmm_host_extirqs *group,
			    unsigned offset)
{
	if (offset > group->count)
		return VMM_ENOTAVAIL;

	return group->base + offset;
}

u32 vmm_host_extirqs_get_offset(struct vmm_host_extirqs *group,
				u32 irq)
{
	return irq - group->base;
}

extern void host_irq_init_irq(struct vmm_host_irq *irq,
			      u32 num);

int vmm_host_extirq_map(u32 hwirq,
			const char *basename,
			u32 size,
			struct vmm_host_irq_chip *chip,
			void *chip_data,
			void *dev,
			struct vmm_host_extirqs **group)
{
	int err = VMM_OK;
	u32 i = 0;
	u32 len = 0;
	char *name = NULL;
	struct vmm_host_irq *irq = NULL;
	irq_flags_t flags;
	struct vmm_host_extirqs* extirqs = NULL;

	/* We support only up to 999 IRQs by extended IRQ group */
	if (size > 999)
		return VMM_ENOTAVAIL;

	if (NULL == (irq = vmm_host_irq_get(hwirq))) {
		vmm_printf("Could not get HW IRQ %d\n", hwirq);
		return VMM_EFAIL;
	}

	if ((NULL == chip) && (NULL == (chip = vmm_host_irq_get_chip(irq)))) {
			vmm_printf("HW IRQ %d chip is not set\n", hwirq);
		return VMM_EFAIL;
	}
	if (NULL == chip_data) {
		chip_data = vmm_host_irq_get_chip_data(irq);
	}

	vmm_spin_lock_irqsave(&extirqctrl.lock, flags);
	if (CONFIG_EXTENDED_IRQ_GROUP_NB <= extirqctrl.count) {
		err = VMM_ENOTAVAIL;
		goto out;
	}

	extirqs = &extirqctrl.groups[extirqctrl.count];
	extirqs->base = extirqctrl.base;
	extirqs->count = size;
	extirqs->hwirq = hwirq;
	extirqs->dev = dev;
	extirqs->irqs = vmm_malloc(size * sizeof (struct vmm_host_irq));

	len = strlen(basename) + 5;
	for (i = 0; i < size; ++i) {
		irq = &extirqs->irqs[i];

		name = vmm_malloc(len);
		vmm_snprintf(name, len, "%s.%d", basename, i + 1);
		host_irq_init_irq(irq, extirqctrl.base);
		irq->name = name;
		irq->chip = chip;
		irq->chip_data = chip_data;
		extirqctrl.irqs[extirqctrl.base - CONFIG_HOST_IRQ_COUNT] = irq;
		++extirqctrl.base;
	}

	++extirqctrl.count;
	*group = extirqs;

out:
	vmm_spin_unlock_irqrestore(&extirqctrl.lock, flags);
	return err;
}

int __cpuinit vmm_host_extirq_init(void)
{
	memset(&extirqctrl, 0, sizeof (struct vmm_host_extirqs_ctrl));
	INIT_SPIN_LOCK(&extirqctrl.lock);
	extirqctrl.base = CONFIG_HOST_IRQ_COUNT;
	extirqctrl.irqs = vmm_malloc(CONFIG_EXTENDED_IRQ_NB *
				     sizeof (struct vmm_host_irq *));

	return VMM_OK;
}
