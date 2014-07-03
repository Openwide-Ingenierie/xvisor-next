/*
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
 * Generic MMIO clocksource support, adapted from the Linux Kernel 3.13.6
 * drivers/clocksource/mmio.c file.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * @file mmio.c
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief Generic MMIO clocksource support.
 */
#include <vmm_clocksource.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>

#define readl_relaxed	readl
#define writel_relaxed	writel
#define readw_relaxed	readw
#define writew_relaxed	writew

struct clocksource_mmio {
	void __iomem *reg;
	struct vmm_clocksource clksrc;
};

static inline struct clocksource_mmio *to_mmio_clksrc(struct vmm_clocksource *c)
{
	return container_of(c, struct clocksource_mmio, clksrc);
}

u64 clocksource_mmio_readl_up(struct vmm_clocksource *c)
{
	return readl_relaxed(to_mmio_clksrc(c)->reg);
}

u64 clocksource_mmio_readl_down(struct vmm_clocksource *c)
{
	return ~readl_relaxed(to_mmio_clksrc(c)->reg);
}

u64 clocksource_mmio_readw_up(struct vmm_clocksource *c)
{
	return readw_relaxed(to_mmio_clksrc(c)->reg);
}

u64 clocksource_mmio_readw_down(struct vmm_clocksource *c)
{
	return ~(unsigned)readw_relaxed(to_mmio_clksrc(c)->reg);
}

/**
 * clocksource_mmio_init - Initialize a simple mmio based clocksource
 * @base:	Virtual address of the clock readout register
 * @name:	Name of the clocksource
 * @hz:		Frequency of the clocksource in Hz
 * @rating:	Rating of the clocksource
 * @bits:	Number of valid bits
 * @read:	One of clocksource_mmio_read*() above
 */
int __init clocksource_mmio_init(void __iomem *base, const char *name,
	unsigned long hz, int rating, unsigned bits,
	u64 (*read)(struct vmm_clocksource *))
{
	struct clocksource_mmio *cs;

	if (bits > 32 || bits < 16)
		return -EINVAL;

	cs = kzalloc(sizeof(struct clocksource_mmio), GFP_KERNEL);
	if (!cs)
		return -ENOMEM;

	cs->reg = base;
	cs->clksrc.name = name;
	cs->clksrc.rating = rating;
	cs->clksrc.read = read;
	cs->clksrc.mask = VMM_CLOCKSOURCE_MASK(bits);
	/* cs->clksrc.flags = CLOCK_SOURCE_IS_CONTINUOUS; */

	/* return clocksource_register_hz(&cs->clksrc, hz); */
	return vmm_clocksource_register(&cs->clksrc);
}
