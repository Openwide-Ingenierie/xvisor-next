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
 * @file m25p80_chardev.c
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief MTD SPI character device driver for ST M25Pxx (and similar) flash
 */

#include <linux/mtd/mtd.h>
#include <vmm_chardev.h>
#include "m25p80.h"



int m25p_chardev_ioctl(struct vmm_chardev *cdev,
		       int cmd, void *buf, u32 len)
{
	return VMM_OK;
}

u32 m25p_chardev_read(struct vmm_chardev *cdev,
		      u8 *dest, u32 len, bool sleep)
{
	return VMM_OK;
}

u32 m25p_chardev_write(struct vmm_chardev *cdev,
		       u8 *src, u32 len, bool sleep)
{
	return VMM_OK;
}

struct vmm_chardev m25p_chardev = {
	.ioctl = m25p_chardev_ioctl,
	.read = m25p_chardev_read,
	.write = m25p_chardev_write,
};

int m25p_register_chardev(struct vmm_device *dev)
{
	int			err = VMM_OK;
	struct spi_device	*spi = vmm_devdrv_get_data(dev);
	struct m25p		*flash = NULL;

	if (!spi)
		return VMM_EFAIL;

	flash = spi_get_drvdata(spi);

	strncpy(m25p_chardev.name, dev->name, VMM_FIELD_NAME_SIZE);
	m25p_chardev.dev.parent = dev;
	m25p_chardev.priv = flash;

	if (VMM_OK != (err = vmm_chardev_register(&m25p_chardev))) {
		dev_warn(dev, "Failed to register MTD chardev\n");
		return err;
	}
	flash->chardev = &m25p_chardev;

	return VMM_OK;
}

int m25p_unregister_chardev(struct vmm_device *dev)
{
	struct spi_device	*spi = vmm_devdrv_get_data(dev);
	struct m25p		*flash = NULL;

	if (!spi)
		return VMM_EFAIL;

	flash = spi_get_drvdata(spi);
	flash->chardev = NULL;

	return vmm_chardev_unregister(&m25p_chardev);
}
