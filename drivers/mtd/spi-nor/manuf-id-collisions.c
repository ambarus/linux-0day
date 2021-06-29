// SPDX-License-Identifier: GPL-2.0
/*
 * Used to handle collisions between manufacturers, where manufacturers are
 * ignorant enough to not implement the ID continuation scheme described in the
 * JEP106 JEDEC standard.
 */

#include <linux/mtd/spi-nor.h>
#include "core.h"

static void boya_nor_late_init(struct spi_nor *nor)
{
	nor->manufacturer_name = "boya";
}

static const struct spi_nor_fixups boya_nor_fixups = {
	.late_init = boya_nor_late_init,
};

static const struct flash_info id_collision_parts[] = {
	/* Boya */
	{ "by25q128as", INFO(0x684018, 0, 64 * 1024, 256)
		FLAGS(SPI_NOR_HAS_LOCK | SPI_NOR_HAS_TB)
		NO_SFDP_FLAGS(SPI_NOR_SKIP_SFDP | SECT_4K | SPI_NOR_DUAL_READ |
			      SPI_NOR_QUAD_READ)
		.fixups = &boya_nor_fixups },
};

const struct spi_nor_manufacturer spi_nor_manuf_id_collisions = {
	.parts = id_collision_parts,
	.nparts = ARRAY_SIZE(id_collision_parts),
};
