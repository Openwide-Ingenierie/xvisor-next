#ifndef _LINUX_IRQDOMAIN_H
#define _LINUX_IRQDOMAIN_H

# include <vmm_host_extirq.h>
# include <linux/of.h>


# define irq_domain_ops	vmm_host_extirq_group_ops
# define irq_domain	vmm_host_extirq_group

static inline
struct irq_domain *irq_domain_add_linear(struct device_node *of_node,
					 unsigned int size,
					 const struct irq_domain_ops *ops,
					 void *host_data)
{
	return vmm_host_extirq_add(of_node, size, ops, host_data);
}

static inline unsigned int irq_create_mapping(struct irq_domain *domain,
					      irq_hw_number_t hwirq)
{
	return vmm_host_extirq_create_mapping(domain, hwirq);
}

static inline void irq_dispose_mapping(unsigned int virq)
{
	return vmm_host_extirq_dispose_mapping(virq);
}

#endif
