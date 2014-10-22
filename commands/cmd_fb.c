/**
 * Copyright (c) 2012 Anup Patel.
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
 * @file cmd_fb.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of fb command
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_devtree.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>
#include <vmm_delay.h>
#include <libs/stringlib.h>
#include <drv/fb.h>

#include "cmd_fb_logo.h"


#define MODULE_DESC			"Command fb"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_fb_init
#define	MODULE_EXIT			cmd_fb_exit

static void cmd_fb_usage(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "Usage:\n");
	vmm_cprintf(cdev, "   fb help\n");
	vmm_cprintf(cdev, "   fb list\n");
	vmm_cprintf(cdev, "   fb info <fb_name>\n");
	vmm_cprintf(cdev, "   fb blank <fb_name> <value>\n");
	vmm_cprintf(cdev, "   fb fillrect <fb_name> <x> <y> <w> <h> <c> "
		    "[<rop>]\n");
	vmm_cprintf(cdev, "   fb logo <fb_name> <x> <y> <w> <h>\n");
}

static void cmd_fb_list(struct vmm_chardev *cdev)
{
	int num, count;
	char path[1024];
	struct fb_info *info;
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
	vmm_cprintf(cdev, " %-16s %-20s %-40s\n", 
			  "Name", "ID", "Device Path");
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
	count = fb_count();
	for (num = 0; num < count; num++) {
		info = fb_get(num);
		if (info->dev.parent && info->dev.parent->node) {
			vmm_devtree_getpath(path, info->dev.parent->node);
		} else {
			strcpy(path, "-----");
		}
		vmm_cprintf(cdev, " %-16s %-20s %-40s\n", 
				  info->name, info->fix.id, path);
	}
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
}

static int cmd_fb_info(struct vmm_chardev *cdev, struct fb_info *info)
{
	const char *str;

	vmm_cprintf(cdev, "Name   : %s\n", info->name);
	vmm_cprintf(cdev, "ID     : %s\n", info->fix.id);

	switch (info->fix.type) {
	case FB_TYPE_PACKED_PIXELS:
		str = "Packed Pixels";
		break;
	case FB_TYPE_PLANES:
		str = "Non interleaved planes";
		break;
	case FB_TYPE_INTERLEAVED_PLANES:
		str = "Interleaved planes";
		break;
	case FB_TYPE_TEXT:
		str = "Text/attributes";
		break;
	case FB_TYPE_VGA_PLANES:
		str = "EGA/VGA planes";
		break;
	default:
		str = "Unknown";
		break;
	};
	vmm_cprintf(cdev, "Type   : %s\n", str);

	switch (info->fix.visual) {
	case FB_VISUAL_MONO01:
		str = "Monochrome 1=Black 0=White";
		break;
	case FB_VISUAL_MONO10:
		str = "Monochrome 0=Black 1=White";
		break;
	case FB_VISUAL_TRUECOLOR:
		str = "True color";
		break;
	case FB_VISUAL_PSEUDOCOLOR:
		str = "Pseudo color";
		break;
	case FB_VISUAL_DIRECTCOLOR:
		str = "Direct color";
		break;
	case FB_VISUAL_STATIC_PSEUDOCOLOR:
		str = "Pseudo color readonly";
		break;
	default:
		str = "Unknown";
		break;
	};
	vmm_cprintf(cdev, "Visual : %s\n", str);

	vmm_cprintf(cdev, "Xres   : %d\n", info->var.xres);
	vmm_cprintf(cdev, "Yres   : %d\n", info->var.yres);
	vmm_cprintf(cdev, "BPP    : %d\n", info->var.bits_per_pixel);

	return VMM_OK;
}

static void fb_dump_mode(struct vmm_chardev *cdev, struct fb_videomode *mode)
{
	vmm_cprintf(cdev, "  %s (refresh %d): %dx%d, pixclk %d\n"
		    "    margins: %d %d %d %d\n"
		    "    hsync %d, vsync %d, sync %d\n"
		    "    vmode %d, flag %d\n",
		    mode->name, mode->refresh, mode->xres, mode->yres,
		    mode->pixclock, mode->left_margin, mode->right_margin,
		    mode->upper_margin, mode->lower_margin, mode->hsync_len,
		    mode->vsync_len, mode->sync, mode->vmode, mode->flag);
}

static int cmd_fb_fillrect(struct vmm_chardev *cdev, struct fb_info *info,
			   int argc, char **argv)
{
	struct dlist *pos;
	struct fb_modelist *modelist;
	struct fb_fillrect rect;
	const struct fb_videomode *hard_mode;
	struct fb_var_screeninfo hard_var;

	if (argc < 5) {
		cmd_fb_usage(cdev);
		return VMM_EFAIL;
	}

	memset(&rect, 0, sizeof (struct fb_fillrect));
	rect.dx = strtol(argv[0], NULL, 10);
	rect.dy = strtol(argv[1], NULL, 10);
	rect.width = strtol(argv[2], NULL, 10);
	rect.height = strtol(argv[3], NULL, 10);
	rect.color = strtol(argv[4], NULL, 16);

	if (rect.color >= (1 << info->var.bits_per_pixel)) {
		vmm_cprintf(cdev, "Color error, %d bpp mode\n",
			    info->var.bits_per_pixel);
		return VMM_EFAIL;
	}

	if (argc > 5) {
		rect.rop = strtol(argv[5], NULL, 10);
	}

	vmm_cprintf(cdev, "Current mode:\n");
	fb_dump_mode(cdev, info->mode);
	vmm_cprintf(cdev, "Modes:\n");

	list_for_each(pos, &info->modelist) {
		modelist = list_entry(pos, struct fb_modelist, list);
		fb_dump_mode(cdev, &modelist->mode);
	}

	hard_var.bits_per_pixel = 24;
	hard_var.xres = 1024;
	hard_var.yres = 768;
	hard_mode = fb_find_best_mode(&hard_var, &info->modelist);
	if (!hard_mode) {
		vmm_cprintf(cdev, "Failed to find mode\n");
		return VMM_EFAIL;
	}

	vmm_cprintf(cdev, "Selected mode:\n");
	fb_dump_mode(cdev, info->mode);

	if (fb_check_var(info, &hard_var)) {
		vmm_cprintf(cdev, "Checking var failed\n");
		return VMM_EFAIL;
	}

	if (fb_set_var(info, &hard_var)) {
		vmm_cprintf(cdev, "Failed setting var\n");
		return VMM_EFAIL;
	}

	if (!info->fbops || !info->fbops->fb_fillrect) {
		vmm_cprintf(cdev, "FB fillrect operation not defined\n");
		return VMM_EFAIL;
	}
	vmm_cprintf(cdev, "X: %d, Y: %d, W: %d, H: %d, color: %d\n",
		    rect.dx, rect.dy, rect.width, rect.height, rect.color);
	info->fbops->fb_fillrect(info, &rect);

	return VMM_OK;
}

typedef enum {
	VSCREEN_COLOR_BLACK,
	VSCREEN_COLOR_RED,
	VSCREEN_COLOR_GREEN,
	VSCREEN_COLOR_YELLOW,
	VSCREEN_COLOR_BLUE,
	VSCREEN_COLOR_MAGENTA,
	VSCREEN_COLOR_CYAN,
	VSCREEN_COLOR_WHITE,
} vscreen_color;
#define VSCREEN_DEFAULT_FC	VSCREEN_COLOR_WHITE
#define VSCREEN_DEFAULT_BC	VSCREEN_COLOR_BLACK

struct fb_bitfield logo[4] = {
	{ 0, 8, 0}, { 8, 8, 0}, {16, 8, 0}, { 0, 0, 0}
};

/**
 * Display images on the framebuffer.
 * The image and the framebuffer must have the same color space and color map.
 */
static int fb_write_image(struct fb_info *info, const struct fb_image *image,
			  unsigned int x, unsigned int y, unsigned int w,
			  unsigned int h)
{
	const char *data = image->data;
	char *screen = info->screen_base;
	unsigned int img_stride = image->width * image->depth / 8;
	unsigned int screen_stride = info->fix.line_length;

	x *= image->depth / 8;

	if (0 == w)
		w = img_stride;
	else
		w *= image->depth / 8;

	if (unlikely(w > screen_stride))
		w = screen_stride;

	if (0 == h)
		h = image->height;

	screen += screen_stride * y;

	while (h--) {
		memcpy(screen + x, data, w);
		data += img_stride;
		screen += screen_stride;
	}

	return VMM_OK;
}

static int cmd_fb_logo(struct vmm_chardev *cdev, struct fb_info *info,
		       int argc, char *argv[])
{
#ifndef CONFIG_CMD_FB_LOGO
	vmm_cprintf(cdev, "Logo option is not enabled.\n");
	return VMM_EFAIL;
#else /* CONFIG_CMD_FB_LOGO */
	unsigned int x = 0;
	unsigned int y = 0;
	unsigned int w = 0;
	unsigned int h = 0;

	if (!info->fbops || !info->fbops->fb_blank) {
		vmm_cprintf(cdev, "FB 'blank' operation not defined\n");
		return VMM_EFAIL;
	}

	if (info->fbops->fb_blank(FB_BLANK_UNBLANK, info)) {
		vmm_cprintf(cdev, "FB 'blank' operation failed\n");
		return VMM_EFAIL;
	}

	if (argc >= 1)
		x = strtol(argv[0], NULL, 10);

	if (argc >= 2)
		y = strtol(argv[1], NULL, 10);

	if (argc >= 3)
		w = strtol(argv[2], NULL, 10);

	if (argc >= 4)
		h = strtol(argv[3], NULL, 10);

	return fb_write_image(info, &cmd_fb_logo_image, x, y, w, h);
#endif /* CONFIG_CMD_FB_LOGO */
}

static int cmd_fb_blank(struct vmm_chardev *cdev, struct fb_info *info,
			int argc, char **argv)
{
	int blank = 0;

	if (argc < 1) {
		cmd_fb_usage(cdev);
		return VMM_EFAIL;
	}

	if (!info->fbops || !info->fbops->fb_blank) {
		vmm_cprintf(cdev, "FB 'blank' operation not defined\n");
		return VMM_EFAIL;
	}

	blank = strtol(argv[0], NULL, 10);

	switch (blank) {
	case FB_BLANK_POWERDOWN:
		vmm_cprintf(cdev, "Setting '%s' blank to power down\n",
			    info->name);
		break;
	case FB_BLANK_VSYNC_SUSPEND:
		vmm_cprintf(cdev, "Setting '%s' blank to vsync suspend\n",
			    info->name);
		break;
	case FB_BLANK_HSYNC_SUSPEND:
		vmm_cprintf(cdev, "Setting '%s' blank to hsync suspend\n",
			    info->name);
		break;
	case FB_BLANK_NORMAL:
		vmm_cprintf(cdev, "Setting '%s' blank to normal\n",
			    info->name);
		break;
	case FB_BLANK_UNBLANK:
		vmm_cprintf(cdev, "Setting '%s' blank to unblank\n",
			    info->name);
		break;
	}

	if (info->fbops->fb_blank(blank, info)) {
		return VMM_EFAIL;
	}
	return VMM_OK;
}

static int cmd_fb_exec(struct vmm_chardev *cdev, int argc, char **argv)
{
	struct fb_info *info = NULL;

	if (argc == 2) {
		if (strcmp(argv[1], "help") == 0) {
			cmd_fb_usage(cdev);
			return VMM_OK;
		} else if (strcmp(argv[1], "list") == 0) {
			cmd_fb_list(cdev);
			return VMM_OK;
		}
	}
	if (argc <= 2) {
		cmd_fb_usage(cdev);
		return VMM_EFAIL;
	}

	info = fb_find(argv[2]);
	if (!info) {
		vmm_cprintf(cdev, "Error: Invalid FB %s\n", argv[2]);
		return VMM_EFAIL;
	}

	if (strcmp(argv[1], "info") == 0) {
		return cmd_fb_info(cdev, info);
	} else if (0 == strcmp(argv[1], "blank")) {
		return cmd_fb_blank(cdev, info, argc - 3, argv + 3);
	} else if (0 == strcmp(argv[1], "fillrect")) {
		return cmd_fb_fillrect(cdev, info, argc - 3, argv + 3);
	} else if (0 == strcmp(argv[1], "logo")) {
		return cmd_fb_logo(cdev, info, argc - 3, argv + 3);
	}
	return VMM_EFAIL;
}

static struct vmm_cmd cmd_fb = {
	.name = "fb",
	.desc = "frame buffer commands",
	.usage = cmd_fb_usage,
	.exec = cmd_fb_exec,
};

static int __init cmd_fb_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_fb);
}

static void __exit cmd_fb_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_fb);
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
