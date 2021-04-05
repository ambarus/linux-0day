// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2005, Intec Automation Inc.
 * Copyright (C) 2014, Freescale Semiconductor, Inc.
 */

#include <linux/bitfield.h>
#include <linux/mtd/spi-nor.h>

#include "core.h"

#define SPINOR_OP_SST26_RBPR	0x72	/* Read Block-Protection Register */
#define SPINOR_OP_SST26_WBPR	0x42	/* Write Block-Protection Register */


#define SST26VF_CR_BPNV		BIT(3)

/*
 * SST26 Memory Organization:
 * 4 *  8 KByte blocks (read and write protection bits)
 * 1 * 32 KByte block  (write protection bits)
 * (mtd->size / SZ_64K - 2) * 64Kbyte blocks (write protection bits)
 * 1 * 32 KByte block  (write protection bits)
 * 4 *  8 KByte blocks (read and write protection bits)
 */
#define spi_nor_sst26_bpr_len(n_sectors)	\
	((((n_sectors) - 2 + 2 + 2 * (4 + 4)) + BITS_PER_BYTE - 1) / BITS_PER_BYTE)

/**
 * spi_nor_sst_rbpr() - Read Block-Protection Register on SPI NOR SST26 family.
 * @nor:	pointer to 'struct spi_nor'.
 * @bpr:	pointer to DMA-able buffer where the value of the
 *		Block-Protection Register will be written.
 * @len:	number of bytes to write to the Block-Protection Register.
 *
 * Return: 0 on success, -errno otherwise.
 */
static int spi_nor_sst26_rbpr(struct spi_nor *nor, u8 *bpr, size_t len)
{
	int ret;

	if (nor->spimem) {
		struct spi_mem_op op =
			SPI_MEM_OP(SPI_MEM_OP_CMD(SPINOR_OP_SST26_RBPR, 0),
				   SPI_MEM_OP_NO_ADDR,
				   SPI_MEM_OP_NO_DUMMY,
				   SPI_MEM_OP_DATA_IN(len, bpr, 0));

		spi_nor_spimem_setup_op(nor, &op, nor->reg_proto);

		ret = spi_mem_exec_op(nor->spimem, &op);
	} else {
		ret = spi_nor_controller_ops_read_reg(nor, SPINOR_OP_SST26_RBPR,
						      bpr, len);
	}

	if (ret)
		dev_dbg(nor->dev, "error %d reading SST26 BPR\n", ret);

	return ret;
}

/**
 * spi_nor_sst26_wbpr() - Write Block-Protection Register on
 *			SPI NOR SST26 family.
 * @nor:	pointer to 'struct spi_nor'.
 * @bpr:	pointer to DMA-able buffer to write to the Block-Protection
 *		Register.
 * @len:	number of bytes to write to the Block-Protection Register.
 *
 * Return: 0 on success, -errno otherwise.
 */
static int spi_nor_sst26_wbpr(struct spi_nor *nor, const u8 *bpr, size_t len)
{
	int ret;

	ret = spi_nor_write_enable(nor);
	if (ret)
		return ret;

	if (nor->spimem) {
		struct spi_mem_op op =
			SPI_MEM_OP(SPI_MEM_OP_CMD(SPINOR_OP_SST26_WBPR, 0),
				   SPI_MEM_OP_NO_ADDR,
				   SPI_MEM_OP_NO_DUMMY,
				   SPI_MEM_OP_DATA_OUT(len, bpr, 0));

		spi_nor_spimem_setup_op(nor, &op, nor->reg_proto);

		ret = spi_mem_exec_op(nor->spimem, &op);
	} else {
		ret = spi_nor_controller_ops_write_reg(nor,
						       SPINOR_OP_SST26_WBPR,
						       bpr, len);
	}

	if (ret) {
		dev_dbg(nor->dev, "error %d writing SST26 BPR\n", ret);
		return ret;
	}

	return spi_nor_wait_till_ready(nor);
}

static int sst26vf_lock(struct spi_nor *nor, loff_t ofs, uint64_t len)
{
	return -EOPNOTSUPP;
}

static int sst26vf_unlock(struct spi_nor *nor, loff_t ofs, uint64_t len)
{
	int ret;

	/* We only support unlocking the entire flash array. */
	if (ofs != 0 || len != nor->params->size)
		return -EINVAL;

	ret = spi_nor_read_cr(nor, nor->bouncebuf);
	if (ret)
		return ret;

	if (!(nor->bouncebuf[0] & SST26VF_CR_BPNV)) {
		dev_dbg(nor->dev, "Any block has been permanently locked\n");
		return -EINVAL;
	}

	return spi_nor_global_block_unlock(nor);
}

static int sst26vf_is_locked(struct spi_nor *nor, loff_t ofs, uint64_t len)
{
	return -EOPNOTSUPP;
}

static const struct spi_nor_locking_ops sst26vf_locking_ops = {
	.lock = sst26vf_lock,
	.unlock = sst26vf_unlock,
	.is_locked = sst26vf_is_locked,
};

static void sst26vf_default_init(struct spi_nor *nor)
{
	nor->params->locking_ops = &sst26vf_locking_ops;
}

static const struct spi_nor_fixups sst26vf_fixups = {
	.default_init = sst26vf_default_init,
};

static const struct flash_info sst_parts[] = {
	/* SST -- large erase sizes are "overlays", "sectors" are 4K */
	{ "sst25vf040b", INFO(0xbf258d, 0, 64 * 1024,  8,
			      SECT_4K | SST_WRITE | SPI_NOR_HAS_LOCK | SPI_NOR_SWP_IS_VOLATILE) },
	{ "sst25vf080b", INFO(0xbf258e, 0, 64 * 1024, 16,
			      SECT_4K | SST_WRITE | SPI_NOR_HAS_LOCK | SPI_NOR_SWP_IS_VOLATILE) },
	{ "sst25vf016b", INFO(0xbf2541, 0, 64 * 1024, 32,
			      SECT_4K | SST_WRITE | SPI_NOR_HAS_LOCK | SPI_NOR_SWP_IS_VOLATILE) },
	{ "sst25vf032b", INFO(0xbf254a, 0, 64 * 1024, 64,
			      SECT_4K | SST_WRITE | SPI_NOR_HAS_LOCK | SPI_NOR_SWP_IS_VOLATILE) },
	{ "sst25vf064c", INFO(0xbf254b, 0, 64 * 1024, 128,
			      SECT_4K | SPI_NOR_4BIT_BP | SPI_NOR_HAS_LOCK |
			      SPI_NOR_SWP_IS_VOLATILE) },
	{ "sst25wf512",  INFO(0xbf2501, 0, 64 * 1024,  1,
			      SECT_4K | SST_WRITE | SPI_NOR_HAS_LOCK | SPI_NOR_SWP_IS_VOLATILE) },
	{ "sst25wf010",  INFO(0xbf2502, 0, 64 * 1024,  2,
			      SECT_4K | SST_WRITE | SPI_NOR_HAS_LOCK | SPI_NOR_SWP_IS_VOLATILE) },
	{ "sst25wf020",  INFO(0xbf2503, 0, 64 * 1024,  4,
			      SECT_4K | SST_WRITE | SPI_NOR_HAS_LOCK | SPI_NOR_SWP_IS_VOLATILE) },
	{ "sst25wf020a", INFO(0x621612, 0, 64 * 1024,  4, SECT_4K | SPI_NOR_HAS_LOCK) },
	{ "sst25wf040b", INFO(0x621613, 0, 64 * 1024,  8, SECT_4K | SPI_NOR_HAS_LOCK) },
	{ "sst25wf040",  INFO(0xbf2504, 0, 64 * 1024,  8,
			      SECT_4K | SST_WRITE | SPI_NOR_HAS_LOCK | SPI_NOR_SWP_IS_VOLATILE) },
	{ "sst25wf080",  INFO(0xbf2505, 0, 64 * 1024, 16,
			      SECT_4K | SST_WRITE | SPI_NOR_HAS_LOCK | SPI_NOR_SWP_IS_VOLATILE) },
	{ "sst26wf016b", INFO(0xbf2651, 0, 64 * 1024, 32,
			      SECT_4K | SPI_NOR_DUAL_READ |
			      SPI_NOR_QUAD_READ) },
	{ "sst26vf016b", INFO(0xbf2641, 0, 64 * 1024, 32,
			      SECT_4K | SPI_NOR_DUAL_READ) },
	{ "sst26vf064b", INFO(0xbf2643, 0, 64 * 1024, 128,
			      SECT_4K | SPI_NOR_DUAL_READ | SPI_NOR_QUAD_READ |
			      SPI_NOR_HAS_LOCK | SPI_NOR_SWP_IS_VOLATILE)
		.fixups = &sst26vf_fixups },
};

static int sst_write(struct mtd_info *mtd, loff_t to, size_t len,
		     size_t *retlen, const u_char *buf)
{
	struct spi_nor *nor = mtd_to_spi_nor(mtd);
	size_t actual = 0;
	int ret;

	dev_dbg(nor->dev, "to 0x%08x, len %zd\n", (u32)to, len);

	ret = spi_nor_lock_and_prep(nor);
	if (ret)
		return ret;

	ret = spi_nor_write_enable(nor);
	if (ret)
		goto out;

	nor->sst_write_second = false;

	/* Start write from odd address. */
	if (to % 2) {
		nor->program_opcode = SPINOR_OP_BP;

		/* write one byte. */
		ret = spi_nor_write_data(nor, to, 1, buf);
		if (ret < 0)
			goto out;
		WARN(ret != 1, "While writing 1 byte written %i bytes\n", ret);
		ret = spi_nor_wait_till_ready(nor);
		if (ret)
			goto out;

		to++;
		actual++;
	}

	/* Write out most of the data here. */
	for (; actual < len - 1; actual += 2) {
		nor->program_opcode = SPINOR_OP_AAI_WP;

		/* write two bytes. */
		ret = spi_nor_write_data(nor, to, 2, buf + actual);
		if (ret < 0)
			goto out;
		WARN(ret != 2, "While writing 2 bytes written %i bytes\n", ret);
		ret = spi_nor_wait_till_ready(nor);
		if (ret)
			goto out;
		to += 2;
		nor->sst_write_second = true;
	}
	nor->sst_write_second = false;

	ret = spi_nor_write_disable(nor);
	if (ret)
		goto out;

	ret = spi_nor_wait_till_ready(nor);
	if (ret)
		goto out;

	/* Write out trailing byte if it exists. */
	if (actual != len) {
		ret = spi_nor_write_enable(nor);
		if (ret)
			goto out;

		nor->program_opcode = SPINOR_OP_BP;
		ret = spi_nor_write_data(nor, to, 1, buf + actual);
		if (ret < 0)
			goto out;
		WARN(ret != 1, "While writing 1 byte written %i bytes\n", ret);
		ret = spi_nor_wait_till_ready(nor);
		if (ret)
			goto out;

		actual += 1;

		ret = spi_nor_write_disable(nor);
	}
out:
	*retlen += actual;
	spi_nor_unlock_and_unprep(nor);
	return ret;
}

static void sst_post_sfdp_fixups(struct spi_nor *nor)
{
	if (nor->info->flags & SST_WRITE)
		nor->mtd._write = sst_write;
}

static const struct spi_nor_fixups sst_fixups = {
	.post_sfdp = sst_post_sfdp_fixups,
};

/*
 * The manufacturer specific manufacturer table is uniquely identified by the
 * SFDP_SST_ID. The MSB value (0x01) indicates the bank number of JEDEC JEP106
 * and the LSB value (0xbf) indicates the Manufacturer ID.
 */
#define SFDP_SST_ID		0x01bf

/* SST Manufacturer Parameter Table definitions. */
#define SST_MPT_IBP_MAP_DWORD_OFFSET	0x13
#define SST_MPT_IBP_MAP_SECTIONS_COUNT	5
#define SST_MPT_IBP_MAP_SECTOR_TYPE	GENMASK(7, 0)
#define SST_MPT_IBP_MAP_N_SECTORS	GENMASK(15, 8)
#define SST_MPT_IBP_MAP_BIT_START	GENMASK(23, 16)
#define SST_MPT_IBP_MAP_BIT_END		GENMASK(31, 24)

struct sst_mpt_ibp_section {
	u8 sector_type;
	u8 n_sectors;
	s8 bpr_bit_start;
	s8 bpr_bit_end;
	u32 block_size;
};

struct sst_priv {
	struct sst_mpt_ibp_section section[SST_MPT_IBP_MAP_SECTIONS_COUNT];
};

static int sst_mpt_bpr_map_init(struct spi_nor *nor, const u32 *sst_mpt)
{
	struct sst_priv *sst_priv = nor->params->priv;
	struct sst_mpt_ibp_section *section = sst_priv->section;
	struct spi_nor_erase_type *erase_type =
		nor->params->erase_map.erase_type;
	unsigned int i, j, k;

	j = SST_MPT_IBP_MAP_DWORD_OFFSET;
	for (i = 0; i < SST_MPT_IBP_MAP_SECTIONS_COUNT; i++) {
		section[i].sector_type = FIELD_GET(SST_MPT_IBP_MAP_SECTOR_TYPE,
						   sst_mpt[j]);
		section[i].n_sectors = FIELD_GET(SST_MPT_IBP_MAP_N_SECTORS,
						 sst_mpt[j]);
		section[i].bpr_bit_start = FIELD_GET(SST_MPT_IBP_MAP_BIT_START,
						     sst_mpt[j]);
		section[i].bpr_bit_end = FIELD_GET(SST_MPT_IBP_MAP_BIT_END,
						   sst_mpt[j]);
		for (k = 0; k < SNOR_ERASE_TYPE_MAX; k++)
			if (section[i].sector_type == (erase_type[k].idx + 1))
				section[i].block_size = erase_type[k].size;

		j++;
		dev_err(nor->dev,"section %d sector_type = %02x, n_sectors = "
			"%02x bpr_bit_start = %02x bpr_bit_end = %02x, "
			"block_size = %u\n",
			i, section[i].sector_type, section[i].n_sectors,
			section[i].bpr_bit_start, section[i].bpr_bit_end,
			section[i].block_size);
	}

	return 0;
}

static int sst_manufacturer_specific_sfdp_parse(struct spi_nor *nor,
			const struct sfdp_parameter_header *param_header)
{
	struct sst_priv *sst_priv;
	u32 *sst_mpt;
	size_t size;
	u32 addr;
	int ret;
	unsigned int i;

	size = param_header->length * sizeof(u32);
	sst_mpt = kmalloc(size, GFP_KERNEL);
	if (!sst_mpt)
		return -ENOMEM;

	addr = SFDP_PARAM_HEADER_PTP(param_header);
	ret = spi_nor_read_sfdp(nor, addr, size, sst_mpt);
	if (ret)
		goto out;
        /* Fix endianness of the BFPT DWORDs. */
        le32_to_cpu_array(sst_mpt, param_header->length);

	for (i = 0; i < param_header->length; i++)
		dev_err(nor->dev, "sst_mpt[%d] = %08x\n", i, sst_mpt[i]);

	sst_priv = devm_kzalloc(nor->dev, sizeof(*sst_priv), GFP_KERNEL);
	if (!sst_priv) {
		ret = -ENOMEM;
		goto out;
	}
	nor->params->priv = sst_priv;

	ret = sst_mpt_bpr_map_init(nor, sst_mpt);
	if (ret)
		goto out;

out:
	kfree(sst_mpt);
	return ret;
}

static const struct spi_nor_manufacturer_sfdp sst_sfdp = {
	.id = SFDP_SST_ID,
	.parse = sst_manufacturer_specific_sfdp_parse,
};

const struct spi_nor_manufacturer spi_nor_sst = {
	.name = "sst",
	.parts = sst_parts,
	.nparts = ARRAY_SIZE(sst_parts),
	.fixups = &sst_fixups,
	.sfdp = &sst_sfdp,
};
