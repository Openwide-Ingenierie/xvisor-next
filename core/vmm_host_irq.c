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
 * @file vmm_host_irq.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source code for host interrupts
 */

#include <vmm_error.h>
#include <vmm_smp.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_host_irq.h>
#include <arch_cpu_irq.h>
#include <arch_host_irq.h>
#include <libs/stringlib.h>
#include <vmm_host_extended_irq.h>

struct vmm_host_irqs_ctrl {
	vmm_spinlock_t lock;
	struct vmm_host_irq *irq;
	u32 (*active)(u32);
	const struct vmm_devtree_nodeid *matches;
};

static struct vmm_host_irqs_ctrl hirqctrl;

void vmm_handle_fast_eoi(struct vmm_host_irq *irq, u32 cpu, void *data)
{
	irq_flags_t flags;
	struct vmm_host_irq_action *act;

	vmm_read_lock_irqsave_lite(&irq->action_lock[cpu], flags);
	list_for_each_entry(act, &irq->action_list[cpu], head) {
		if (act->func(irq->num, act->dev) == VMM_IRQ_HANDLED) {
			break;
		}
	}
	vmm_read_unlock_irqrestore_lite(&irq->action_lock[cpu], flags);

	if (irq->chip && irq->chip->irq_eoi) {
		irq->chip->irq_eoi(irq);
	}
}

void vmm_handle_level_irq(struct vmm_host_irq *irq, u32 cpu, void *data)
{
	irq_flags_t flags;
	struct vmm_host_irq_action *act;

	if (irq->chip) {
		if (irq->chip->irq_mask_ack) {
			irq->chip->irq_mask_ack(irq);
		} else {
			if (irq->chip->irq_mask) {
				irq->chip->irq_mask(irq);
			}
			if (irq->chip->irq_ack) {
				irq->chip->irq_ack(irq);
			}
		}
	}

	vmm_read_lock_irqsave_lite(&irq->action_lock[cpu], flags);
	list_for_each_entry(act, &irq->action_list[cpu], head) {
		if (act->func(irq->num, act->dev) == VMM_IRQ_HANDLED) {
			break;
		}
	}
	vmm_read_unlock_irqrestore_lite(&irq->action_lock[cpu], flags);

	if (irq->chip && irq->chip->irq_unmask) {
		irq->chip->irq_unmask(irq);
	}
}

struct vmm_host_irq *vmm_host_irq_get(u32 hirq_num)
{
	if (hirq_num < CONFIG_HOST_IRQ_COUNT) {
		return &hirqctrl.irq[hirq_num];
	}

	return vmm_host_extirq_get(hirq_num);
}

int vmm_host_generic_irq_exec(u32 hirq_no)
{
	u32 cpu;
	struct vmm_host_irq *irq = NULL;

	if (NULL == (irq = vmm_host_irq_get(hirq_no)))
		return VMM_ENOTAVAIL;

	cpu = vmm_smp_processor_id();
	irq->count[cpu]++;
	if (!(irq->state & VMM_IRQ_STATE_PER_CPU)) {
		irq->state |= VMM_IRQ_STATE_INPROGRESS;
	}
	if (irq->handler) {
		irq->handler(irq, cpu, irq->handler_data);
	}
	if (!(irq->state & VMM_IRQ_STATE_PER_CPU)) {
		irq->state &= ~VMM_IRQ_STATE_INPROGRESS;
	}

	return VMM_OK;
}

int vmm_host_active_irq_exec(u32 cpu_irq_no)
{
	u32 hirq_no;

	if (!hirqctrl.active) {
		return VMM_ENOTAVAIL;
	}

	hirq_no = hirqctrl.active(cpu_irq_no);
	while (hirq_no < CONFIG_HOST_IRQ_COUNT) {
		vmm_host_generic_irq_exec(hirq_no);

		hirq_no = hirqctrl.active(cpu_irq_no);
	}

	return VMM_OK;
}

void vmm_host_irq_set_active_callback(u32 (*active)(u32))
{
	hirqctrl.active = active;
}

u32 vmm_host_irq_count(void)
{
	return CONFIG_HOST_IRQ_COUNT;
}

int vmm_host_irq_set_chip(u32 hirq_num, struct vmm_host_irq_chip *chip)
{
	struct vmm_host_irq *irq = NULL;

	if (NULL == (irq = vmm_host_irq_get(hirq_num)))
		return VMM_EFAIL;

	irq->chip = chip;
	return VMM_OK;
}

struct vmm_host_irq_chip *vmm_host_irq_get_chip(struct vmm_host_irq *irq)
{
	return (irq) ? irq->chip : NULL;
}

int vmm_host_irq_set_chip_data(u32 hirq_num, void *chip_data)
{
	struct vmm_host_irq *irq = NULL;

	if (NULL == (irq = vmm_host_irq_get(hirq_num)))
		return VMM_EFAIL;

	irq->chip_data = chip_data;
	return VMM_OK;
}

void *vmm_host_irq_get_chip_data(struct vmm_host_irq *irq)
{
	return (irq) ? irq->chip_data : NULL;
}

int vmm_host_irq_set_handler(u32 hirq_num, vmm_host_irq_handler_t handler)
{
	struct vmm_host_irq *irq = NULL;

	if (NULL == (irq = vmm_host_irq_get(hirq_num)))
		return VMM_EFAIL;

	irq->handler = handler;
	return VMM_OK;
}

vmm_host_irq_handler_t vmm_host_irq_get_handler(u32 hirq_num)
{
	struct vmm_host_irq *irq = NULL;

	if (NULL == (irq = vmm_host_irq_get(hirq_num)))
		return NULL;

	return irq->handler;
}

int vmm_host_irq_set_handler_data(u32 hirq_num, void *data)
{
	struct vmm_host_irq *irq = NULL;

	if (NULL == (irq = vmm_host_irq_get(hirq_num)))
		return VMM_EFAIL;

	irq->handler_data = data;
	return VMM_OK;
}

void *vmm_host_irq_get_handler_data(u32 hirq_num)
{
	struct vmm_host_irq *irq = NULL;

	if (NULL == (irq = vmm_host_irq_get(hirq_num)))
		return NULL;

	return irq->handler_data;
}

int vmm_host_irq_set_affinity(u32 hirq_num, 
			      const struct vmm_cpumask *dest, 
			      bool force)
{
	struct vmm_host_irq *irq;

	if (NULL == (irq = vmm_host_irq_get(hirq_num)))
		return VMM_ENOTAVAIL;

	if (irq->chip && irq->chip->irq_set_affinity) {
		irq->state |= VMM_IRQ_STATE_AFFINITY_SET;
		return irq->chip->irq_set_affinity(irq, dest, force);
	}

	return VMM_EFAIL;
}

int vmm_host_irq_set_type(u32 hirq_num, u32 type)
{
	int rc = VMM_EFAIL;
	struct vmm_host_irq *irq;

	if (NULL == (irq = vmm_host_irq_get(hirq_num)))
		return VMM_ENOTAVAIL;

	type &= VMM_IRQ_TYPE_SENSE_MASK;
	if (type == VMM_IRQ_TYPE_NONE) {
		return VMM_OK;
	}
	if (irq->chip && irq->chip->irq_set_type) {
		rc = irq->chip->irq_set_type(irq, type);
	} else {
		return VMM_OK;
	}
	if (rc == VMM_OK) {
		irq->state &= ~VMM_IRQ_STATE_TRIGGER_MASK;
		irq->state |= type;
		if (type & VMM_IRQ_TYPE_LEVEL_MASK) {
			irq->state |= VMM_IRQ_STATE_LEVEL;
		} else {
			irq->state &= ~VMM_IRQ_STATE_LEVEL;
		}
	}

	return rc;
}

int vmm_host_irq_mark_per_cpu(u32 hirq_num)
{
	struct vmm_host_irq *irq;

	if (NULL == (irq = vmm_host_irq_get(hirq_num)))
		return VMM_ENOTAVAIL;

	irq->state |= VMM_IRQ_STATE_PER_CPU;
	return VMM_OK;
}

int vmm_host_irq_unmark_per_cpu(u32 hirq_num)
{
	struct vmm_host_irq *irq;

	if (NULL == (irq = vmm_host_irq_get(hirq_num)))
		return VMM_ENOTAVAIL;

	irq->state &= ~VMM_IRQ_STATE_PER_CPU;
	return VMM_OK;
}

int vmm_host_irq_mark_guest_routed(u32 hirq_num)
{
	struct vmm_host_irq *irq;

	if (NULL == (irq = vmm_host_irq_get(hirq_num)))
		return VMM_ENOTAVAIL;

	irq->state |= VMM_IRQ_STATE_GUEST_ROUTED;
	return VMM_OK;
}

int vmm_host_irq_unmark_guest_routed(u32 hirq_num)
{
	struct vmm_host_irq *irq;

	if (NULL == (irq = vmm_host_irq_get(hirq_num)))
		return VMM_ENOTAVAIL;

	irq->state &= ~VMM_IRQ_STATE_GUEST_ROUTED;
	return VMM_OK;
}

int vmm_host_irq_enable(u32 hirq_num)
{
	struct vmm_host_irq *irq;

	if (NULL == (irq = vmm_host_irq_get(hirq_num)))
		return VMM_ENOTAVAIL;

	irq->state &= ~VMM_IRQ_STATE_DISABLED;
	if (irq->chip) {
		if (irq->chip->irq_enable) {
			irq->chip->irq_enable(irq);
		} else {
			irq->chip->irq_unmask(irq);
		}
		return VMM_OK;
	}
	return VMM_ENOTAVAIL;
}

int vmm_host_irq_disable(u32 hirq_num)
{
	struct vmm_host_irq *irq;

	if (NULL == (irq = vmm_host_irq_get(hirq_num)))
		return VMM_ENOTAVAIL;

	irq->state |= VMM_IRQ_STATE_DISABLED;
	if (irq->chip) {
		if (irq->chip->irq_disable) {
			irq->chip->irq_disable(irq);
		} else {
			irq->chip->irq_mask(irq);
		}
	}
	irq->state |= VMM_IRQ_STATE_MASKED;
	return VMM_OK;
}

int vmm_host_irq_unmask(u32 hirq_num)
{
	struct vmm_host_irq *irq;

	if (NULL == (irq = vmm_host_irq_get(hirq_num)))
		return VMM_ENOTAVAIL;

	if (irq->chip && irq->chip->irq_unmask) {
		irq->chip->irq_mask(irq);
		irq->state &= ~VMM_IRQ_STATE_MASKED;
	}
	return VMM_OK;
}

int vmm_host_irq_mask(u32 hirq_num)
{
	struct vmm_host_irq *irq;

	if (NULL == (irq = vmm_host_irq_get(hirq_num)))
		return VMM_ENOTAVAIL;

	if (irq->chip && irq->chip->irq_mask) {
		irq->chip->irq_mask(irq);
		irq->state |= VMM_IRQ_STATE_MASKED;
	}
	return VMM_OK;
}

int vmm_host_irq_raise(u32 hirq_num,
		       const struct vmm_cpumask *dest)
{
	struct vmm_host_irq *irq;

	if (NULL == (irq = vmm_host_irq_get(hirq_num)))
		return VMM_ENOTAVAIL;

	if (irq->chip && irq->chip->irq_raise) {
		irq->chip->irq_raise(irq, dest);
	}
	return VMM_OK;
}

static int host_irq_register(struct vmm_host_irq *irq,
			     const char *name,
			     vmm_host_irq_function_t func,
			     void *dev, u32 cpu)
{
	bool found;
	irq_flags_t flags;
	struct vmm_host_irq_action *act;

	vmm_write_lock_irqsave_lite(&irq->action_lock[cpu], flags);

	found = FALSE;
	list_for_each_entry(act, &irq->action_list[cpu], head) {
		if (act->dev == dev) {
			found = TRUE;
			break;
		}
	}
	if (found) {
		vmm_write_unlock_irqrestore_lite(&irq->action_lock[cpu], flags);
		return VMM_EFAIL;
	}

	irq->name = name;
	act = vmm_zalloc(sizeof(struct vmm_host_irq_action));
	if (!act) {
		vmm_write_unlock_irqrestore_lite(&irq->action_lock[cpu], flags);
		return VMM_ENOMEM;
	}
	INIT_LIST_HEAD(&act->head);
	act->func = func;
	act->dev = dev;

	list_add_tail(&act->head, &irq->action_list[cpu]);

	vmm_write_unlock_irqrestore_lite(&irq->action_lock[cpu], flags);

	return VMM_OK;
}

int vmm_host_irq_register(u32 hirq_num, 
			  const char *name,
			  vmm_host_irq_function_t func,
			  void *dev)
{
	int rc;
	u32 cpu;
	struct vmm_host_irq *irq;

	if (NULL == (irq = vmm_host_irq_get(hirq_num)))
		return VMM_ENOTAVAIL;

	if (vmm_host_irq_is_per_cpu(irq)) {
		rc = host_irq_register(irq, name, func, dev,
				       vmm_smp_processor_id());
		if (rc) {
			return rc;
			}
	} else {
		for (cpu = 0; cpu < CONFIG_CPU_COUNT; cpu++) {
			rc = host_irq_register(irq, name, func,
					       dev, cpu);
			if (rc) {
				return rc;
			}
		}
	}
	return vmm_host_irq_enable(hirq_num);
}

static int host_irq_unregister(struct vmm_host_irq *irq, void *dev,
			       u32 cpu, bool *disable)
{
	bool found;
	irq_flags_t flags;
	struct vmm_host_irq_action *act;

	vmm_write_lock_irqsave_lite(&irq->action_lock[cpu], flags);
	found = FALSE;
	list_for_each_entry(act, &irq->action_list[cpu], head) {
		if (act->dev == dev) {
			found = TRUE;
			break;
		}
	}
	if (!found) {
		vmm_write_unlock_irqrestore_lite(&irq->action_lock[cpu], flags);
		return VMM_EFAIL;
	}

	list_del(&act->head);
	vmm_free(act);
	if (list_empty(&irq->action_list[cpu])) {
		*disable = TRUE;
	}

	vmm_write_unlock_irqrestore_lite(&irq->action_lock[cpu], flags);

	return VMM_OK;
}

int vmm_host_irq_unregister(u32 hirq_num, void *dev)
{
	int rc;
	u32 cpu;
	bool disable;
	struct vmm_host_irq *irq;

	if (NULL == (irq = vmm_host_irq_get(hirq_num)))
		return VMM_ENOTAVAIL;

	disable = FALSE;
	if (vmm_host_irq_is_per_cpu(irq)) {
		rc = host_irq_unregister(irq, dev,
					 vmm_smp_processor_id(),
					 &disable);
		if (rc) {
			return rc;
		}
	} else {
		for (cpu = 0; cpu < CONFIG_CPU_COUNT; cpu++) {
			rc = host_irq_unregister(irq, dev, cpu,
						 &disable);
			if (rc) {
				return rc;
			}
		}
	}
	if (disable) {
		return vmm_host_irq_disable(hirq_num);
	}
	return VMM_OK;
}

int __cpuinit __weak arch_host_irq_init(void)
{
	/* Default weak implementation in-case
	 * architecture does not provide one.
	 */
	return VMM_OK;
}

static void __cpuinit host_irq_nidtbl_found(struct vmm_devtree_node *node,
					const struct vmm_devtree_nodeid *match,
					void *data)
{
	int err;
	vmm_host_irq_init_t init_fn = match->data;

	if (!init_fn) {
		return;
	}

	err = init_fn(node);
#ifdef CONFIG_VERBOSE_MODE
	if (err) {
		vmm_printf("%s: CPU%d Init %s node failed (error %d)\n", 
			   __func__, vmm_smp_processor_id(), node->name, err);
	}
#else
	(void)err;
#endif
}


/**
 * Initialize a vmm_host_irq structure
 * Warning: The associated IRQ must be disabled!
 */
void host_irq_init_irq(struct vmm_host_irq *irq,
		       u32 num)
{
	u32 cpu = 0;

	if (!irq)
		return;

	irq->num = num;
	irq->name = NULL;
	irq->state = (VMM_IRQ_TYPE_NONE |
		      VMM_IRQ_STATE_DISABLED |
		      VMM_IRQ_STATE_MASKED);
	for (cpu = 0; cpu < CONFIG_CPU_COUNT; cpu++) {
		irq->count[cpu] = 0;
	}
	irq->chip = NULL;
	irq->chip_data = NULL;
	irq->handler = NULL;
	irq->handler_data = NULL;
	for (cpu = 0; cpu < CONFIG_CPU_COUNT; cpu++) {
		INIT_RW_LOCK(&irq->action_lock[cpu]);
		INIT_LIST_HEAD(&irq->action_list[cpu]);
	}
}

int __cpuinit vmm_host_irq_init(void)
{
	int ret;
	u32 ite;

	if (vmm_smp_is_bootcpu()) {
		/* Clear the memory of control structure */
		memset(&hirqctrl, 0, sizeof(hirqctrl));

		/* Initialize spin lock */
		INIT_SPIN_LOCK(&hirqctrl.lock);
		
		/* Allocate memory for irq array */
		hirqctrl.irq = vmm_malloc(sizeof(struct vmm_host_irq) * 
					  CONFIG_HOST_IRQ_COUNT);

		if (!hirqctrl.irq) {
			return VMM_ENOMEM;
		}

		/* Reset the handler array */
		for (ite = 0; ite < CONFIG_HOST_IRQ_COUNT; ite++) {
			host_irq_init_irq(&hirqctrl.irq[ite], ite);
		}

		/* Determine clockchip matches from nodeid table */
		hirqctrl.matches = 
			vmm_devtree_nidtbl_create_matches("host_irq");
	}

	/* Initialize board specific PIC */
	if ((ret = arch_host_irq_init())) {
		return ret;
	}

	/* Probe all device tree nodes matching 
	 * host irq nodeid table enteries.
	 */
	if (hirqctrl.matches) {
		vmm_devtree_iterate_matching(NULL,
					     hirqctrl.matches,
					     host_irq_nidtbl_found,
					     NULL);
	}

	/* Setup interrupts in CPU */
	if ((ret = arch_cpu_irq_setup())) {
		return ret;
	}

	if (VMM_OK != (ret = vmm_host_extirq_init())) {
		return ret;
	}

	/* Enable interrupts in CPU */
	arch_cpu_irq_enable();

	return VMM_OK;
}
