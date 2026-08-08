// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "synaptics_fw.h"

/**
 * syna_tcm_write_flash()
 *
 * Implement the bootloader command to write specified data to flash memory.
 *
 * If the length of the data to write is not an integer multiple of words,
 * the trailing byte will be discarded.  If the length of the data to write
 * is not an integer number of write blocks, it will be zero-padded to the
 * next write block.
 *
 * @param
 *    [ in] tcm_dev:      the device handle
 *    [ in] reflash_data: data blob for reflash
 *    [ in] address:      the address in flash memory to write
 *    [ in] wr_data:      data to write
 *    [ in] wr_len:       length of data to write
 *    [ in] wr_delay_ms:  a short delay after the command executed
 *                        set '0' to use default time, which is 20 ms;
 *                        set 'FORCE_ATTN_DRIVEN' to adopt ATTN-driven.
 *
 * @return
 *    on success, 0 or positive value; otherwise, negative value on error.
 */
static int synaptics_ts_write_flash_origin(struct synaptics_ts_data *ts,
		struct synaptics_ts_reflash_data_blob *reflash_data,
		unsigned int address, const unsigned char *wr_data,
		unsigned int wr_len, unsigned int wr_delay_ms)
{
	int retval;
	unsigned int offset;
	unsigned int w_length;
	unsigned int xfer_length;
	unsigned int remaining_length;
	unsigned int flash_address;
	unsigned int block_address;
	unsigned int num_blocks;
	unsigned int resp_delay;

	if (!ts) {
		pr_err("%s%s: Invalid ts device handle\n", SECLOG, __func__);
		return -EINVAL;
	}

	/* ensure that the length to write is the multiple of max_write_payload_size */
	w_length = reflash_data->max_write_payload_size;

	w_length = w_length - (w_length % reflash_data->write_block_size);

	offset = 0;

	remaining_length = wr_len;

	synaptics_ts_buf_lock(&reflash_data->out);

	while (remaining_length) {
		if (remaining_length > w_length)
			xfer_length = w_length;
		else
			xfer_length = remaining_length;

		retval = synaptics_ts_buf_alloc(&reflash_data->out,
				xfer_length + 2);
		if (retval < 0) {
			input_err(true, ts->dev, "%s: Fail to allocate memory for buf.out\n", __func__);
			synaptics_ts_buf_unlock(&reflash_data->out);
			return retval;
		}

		flash_address = address + offset;
		block_address = flash_address / reflash_data->write_block_size;
		reflash_data->out.buf[0] = (unsigned char)block_address;
		reflash_data->out.buf[1] = (unsigned char)(block_address >> 8);

		num_blocks = synaptics_ts_pal_ceil_div(xfer_length, reflash_data->write_block_size);

		if (wr_delay_ms == FORCE_ATTN_DRIVEN) {
			input_dbg(true, ts->dev, "%s: xfer: %d (blocks: %d), delay: ATTN-driven\n", __func__, xfer_length, num_blocks);
			resp_delay = FORCE_ATTN_DRIVEN;
		} else {
			resp_delay = (wr_delay_ms * num_blocks) / 1000;
			input_dbg(true, ts->dev, "%s: xfer: %d (blocks: %d), delay: %d ms\n", __func__, xfer_length, num_blocks, resp_delay);
		}

		retval = synaptics_ts_memcpy(&reflash_data->out.buf[2],
				reflash_data->out.buf_size - 2,
				&wr_data[offset],
				wr_len - offset,
				xfer_length);
		if (retval < 0) {
			input_err(true, ts->dev, "%s: Fail to copy write data ,size: %d\n", __func__,
				xfer_length);
			synaptics_ts_buf_unlock(&reflash_data->out);
			return retval;
		}

		retval = synaptics_ts_reflash_send_command(ts,
				SYNAPTICS_TS_CMD_WRITE_FLASH,
				reflash_data->out.buf,
				xfer_length + 2,
				resp_delay);
		if (retval < 0) {
			input_err(true, ts->dev, "%s: Fail to write data to flash addr 0x%x, size %d\n", __func__,
				flash_address, xfer_length + 2);
			synaptics_ts_buf_unlock(&reflash_data->out);
			return retval;
		}

		offset += xfer_length;
		remaining_length -= xfer_length;
	}

	synaptics_ts_buf_unlock(&reflash_data->out);
	return 0;
}

/**
 * synaptics_ts_write_flash_opt()
 *
 * Implement the bootloader command to write specified data to flash memory.
 *
 * If the length of the data to write is not an integer multiple of words,
 * the trailing byte will be discarded.  If the length of the data to write
 * is not an integer number of write blocks, it will be zero-padded to the
 * next write block.
 *
 * @param
 *    [ in] tcm_dev:      the device handle
 *    [ in] reflash_data: data blob for reflash
 *    [ in] address:      the address in flash memory to write
 *    [ in] wr_data:      data to write
 *    [ in] wr_len:       length of data to write
 *    [ in] wr_delay_ms:  a short delay after the command executed
 *                        set '0' to use default time, which is 20 ms;
 *                        set 'FORCE_ATTN_DRIVEN' to adopt ATTN-driven.
 *
 * @return
 *    on success, 0 or positive value; otherwise, negative value on error.
 */
static int synaptics_ts_write_flash_opt(struct synaptics_ts_data *ts,
		struct synaptics_ts_reflash_data_blob *reflash_data,
		unsigned int address, const unsigned char *wr_data,
		unsigned int wr_len, unsigned int wr_delay_ms)
{
	int retval;
	unsigned int offset;
	unsigned int w_length;
	unsigned int xfer_length;
	unsigned int remaining_length;
	unsigned int flash_address;
	unsigned int start_address;
	unsigned int end_address;
	unsigned int num_blocks;
	unsigned int resp_delay;

	if (!ts) {
		pr_err("%s%s: Invalid ts device handle\n", SECLOG, __func__);
		return -EINVAL;
	}

	if (ts->is_dualization_ic == false) {
		input_err(true, ts->dev, "%s: Optimized write operation not supported\n", __func__);
		return -EINVAL;
	}

	/* ensure that the length to write is the multiple of max_write_payload_size */
	w_length = reflash_data->max_write_payload_size;

	w_length = w_length - (w_length % reflash_data->write_block_size);

	start_address = address / reflash_data->write_block_size;
	end_address = start_address + synaptics_ts_pal_ceil_div(wr_len, reflash_data->write_block_size);

	offset = 0;

	remaining_length = wr_len;

	synaptics_ts_buf_lock(&reflash_data->out);

	while (remaining_length) {
		if (remaining_length > w_length)
			xfer_length = w_length;
		else
			xfer_length = remaining_length;

		retval = synaptics_ts_buf_alloc(&reflash_data->out,
				xfer_length + 6);
		if (retval < 0) {
			input_err(true, ts->dev, "%s: Fail to allocate memory for buf.out\n", __func__);
			synaptics_ts_buf_unlock(&reflash_data->out);
			return retval;
		}

		flash_address = address + offset;
		start_address = flash_address / reflash_data->write_block_size;

		if (remaining_length == wr_len)
			reflash_data->out.buf[0] = 0x01;
		else
			reflash_data->out.buf[0] = 0x00;

		reflash_data->out.buf[2] = (unsigned char)start_address;
		reflash_data->out.buf[3] = (unsigned char)(start_address >> 8);

		reflash_data->out.buf[4] = (unsigned char)end_address;
		reflash_data->out.buf[5] = (unsigned char)(end_address >> 8);

		num_blocks = synaptics_ts_pal_ceil_div(xfer_length, reflash_data->write_block_size);

		num_blocks = synaptics_ts_pal_ceil_div(xfer_length, reflash_data->write_block_size);

		if (wr_delay_ms == FORCE_ATTN_DRIVEN) {
			input_dbg(true, ts->dev, "%s: xfer: %d (blocks: %d), delay: ATTN-driven\n", __func__, xfer_length, num_blocks);
			resp_delay = FORCE_ATTN_DRIVEN;
		} else {
			resp_delay = (wr_delay_ms * num_blocks) / 1000;
			input_dbg(true, ts->dev, "%s: xfer: %d (blocks: %d), delay: %d ms\n", __func__, xfer_length, num_blocks, resp_delay);
		}

		retval = synaptics_ts_memcpy(&reflash_data->out.buf[6],
				reflash_data->out.buf_size - 6,
				&wr_data[offset],
				wr_len - offset,
				xfer_length);
		if (retval < 0) {
			input_err(true, ts->dev, "%s: Fail to copy write data ,size: %d\n", __func__,
				xfer_length);
			synaptics_ts_buf_unlock(&reflash_data->out);
			return retval;
		}

		retval = synaptics_ts_reflash_send_command(ts,
				SYNAPTICS_TS_CMD_OPTIMIZED_WRITE_FLASH,
				reflash_data->out.buf,
				xfer_length + 6,
				resp_delay);
		if (retval < 0) {
			input_err(true, ts->dev, "%s: Fail to write data to flash addr 0x%x, size %d\n", __func__,
				flash_address, xfer_length + 6);
			synaptics_ts_buf_unlock(&reflash_data->out);
			return retval;
		}

		offset += xfer_length;
		remaining_length -= xfer_length;
	}

	synaptics_ts_buf_unlock(&reflash_data->out);
	return 0;
}

/**
 * syna_tcm_write_flash_block()
 *
 * Write data to the target block data area in the flash memory.
 *
 * @param
 *    [ in] tcm_dev:       the device handle
 *    [ in] reflash_data:  data blob for reflash
 *    [ in] area:          target block area to write
 *    [ in] resp_reading:  method to read in the response
 *                          a positive value presents the us time delay for the processing
 *                          of each BLOCKs in the flash to write;
 *                          or, set '0' or 'RESP_IN_ATTN' for ATTN driven
 *    [ in] opt_write:     flag to do optimized write
 *
 * @return
 *    on success, 0 or positive value; otherwise, negative value on error.
 */
static int synaptics_ts_write_flash_block_opt(struct synaptics_ts_data *ts,
		struct synaptics_ts_reflash_data_blob *reflash_data,
		struct block_data *block, unsigned int resp_reading, bool opt_write)
{
	int retval;
	unsigned int size;
	unsigned int flash_addr;
	const unsigned char *data;

	if (!ts) {
		pr_err("%s%s: Invalid ts device handle\n", SECLOG, __func__);
		return -EINVAL;
	}

	if (!reflash_data) {
		input_err(true, ts->dev, "%s: Invalid reflash data blob\n", __func__);
		return -EINVAL;
	}

	if (!block) {
		input_err(true, ts->dev, "%s: Invalid block data\n", __func__);
		return -EINVAL;
	}

	data = block->data;
	size = block->size;
	flash_addr = block->flash_addr;

	input_err(true, ts->dev, "%s: Write data to %s - address: 0x%x, size: %d\n", __func__,
		AREA_ID_STR(block->id), flash_addr, size);

	if (size == 0) {
		input_err(true, ts->dev, "%s: No need to update, size = %d\n", __func__, size);
		goto exit;
	}

	if (opt_write)
		retval = synaptics_ts_write_flash_opt(ts, reflash_data, flash_addr, data, size, resp_reading);
	else
		retval = synaptics_ts_write_flash_origin(ts, reflash_data, flash_addr, data, size, resp_reading);

	if (retval < 0) {
		input_err(true, ts->dev, "%s: Fail to write %s to flash (addr: 0x%x, size: %d)\n", __func__,
			AREA_ID_STR(block->id), flash_addr, size);
		return retval;
	}

exit:
	input_err(true, ts->dev, "%s: %s area written\n", __func__, AREA_ID_STR(block->id));

	return 0;
}

/**
 * syna_tcm_erase_flash()
 *
 * Implement the bootloader command, which is used to erase the specified
 * blocks of flash memory.
 *
 * Until this command completes, the device may be unresponsive.
 * Therefore, this helper is implemented as a blocked function, and the delay
 * time is set to DEFAULT_FLASH_ERASE_DELAY_MS in default.
 *
 * @param
 *    [ in] tcm_dev:        the device handle
 *    [ in] reflash_data:   data blob for reflash
 *    [ in] address:        the address in flash memory to read
 *    [ in] size:           size of data to write
 *    [ in] erase_delay_ms: a short delay after the command executed
 *                          set a positive value or 'DEFAULT_FLASH_ERASE_DELAY' to use default;
 *                          set '0' or 'RESP_IN_ATTN' to select ATTN-driven.
 *
 * @return
 *    on success, 0 or positive value; otherwise, negative value on error.
 */
static int synaptics_ts_erase_flash_opt(struct synaptics_ts_data *ts,
		struct synaptics_ts_reflash_data_blob *reflash_data,
		unsigned int address, unsigned int size,
		unsigned int erase_delay_ms)
{
	int retval;
	unsigned int page_start = 0;
	unsigned int page_count = 0;
	unsigned char out_buf[4] = {0};
	int size_erase_cmd;
	unsigned int resp_delay;

	page_start = address / reflash_data->page_size;

	page_count = (size + reflash_data->page_size - 1) / reflash_data->page_size;

	if (erase_delay_ms == FORCE_ATTN_DRIVEN)
		resp_delay = FORCE_ATTN_DRIVEN;
	else
		resp_delay = erase_delay_ms * page_count;

	input_err(true, ts->dev, "%s: Page start = %d (0x%04x), Page count = %d (0x%04x)\n", __func__,
		page_start, page_start, page_count, page_count);

	if ((page_start > 0xff) || (page_count > 0xff)) {
		size_erase_cmd = 4;

		out_buf[0] = (unsigned char)(page_start & 0xff);
		out_buf[1] = (unsigned char)((page_start >> 8) & 0xff);
		out_buf[2] = (unsigned char)(page_count & 0xff);
		out_buf[3] = (unsigned char)((page_count >> 8) & 0xff);
	} else {
		size_erase_cmd = 2;

		out_buf[0] = (unsigned char)(page_start & 0xff);
		out_buf[1] = (unsigned char)(page_count & 0xff);
	}

	retval = synaptics_ts_reflash_send_command(ts,
			SYNAPTICS_TS_CMD_ERASE_FLASH,
			out_buf,
			size_erase_cmd,
			resp_delay);
	if (retval < 0) {
		input_err(true, ts->dev, "%s: Fail to erase data at flash page 0x%x, count %d\n", __func__,
			page_start, page_count);
		return retval;
	}

	return 0;
}

/**
 * syna_tcm_erase_flash_block()
 *
 * Mass erase the target block data area in the flash memory.
 *
 * @param
 *    [ in] tcm_dev:      the device handle
 *    [ in] reflash_data: data blob for reflash
 *    [ in] block:        target block area to erase
 *    [ in] resp_reading:  method to read in the response
 *                          a positive value presents the us time delay for the processing
 *                          of each PAGEs in the flash to erase;
 *                          or, set '0' or 'RESP_IN_ATTN' for ATTN driven
 *    [ in] opt_write:     flag to do optimized write
 *
 * @return
 *    on success, 0 or positive value; otherwise, negative value on error.
 */
static int synaptics_ts_erase_flash_block_opt(struct synaptics_ts_data *ts,
		struct synaptics_ts_reflash_data_blob *reflash_data,
		struct block_data *block, unsigned int resp_reading, bool opt_write)
{
	int retval;
	unsigned int size;
	unsigned int flash_addr;

	if (!ts) {
		pr_err("%s%s: Invalid ts device handle\n", SECLOG, __func__);
		return -EINVAL;
	}

	if (!reflash_data) {
		input_err(true, ts->dev, "%s: Invalid reflash data blob\n", __func__);
		return -EINVAL;
	}

	if (!block) {
		input_err(true, ts->dev, "%s: Invalid block data\n", __func__);
		return -EINVAL;
	}

	if (opt_write) {
		input_info(true, ts->dev, "%s: bypass %s area due to the optimized write\n", __func__, AREA_ID_STR(block->id));
		return 0;
	}

	flash_addr = block->flash_addr;

	size = block->size;

	input_info(true, ts->dev, "%s: Erase %s block - address: 0x%x, size: %d\n", __func__,
		AREA_ID_STR(block->id), flash_addr, size);

	if (size == 0) {
		input_info(true, ts->dev, "%s: No need to erase, size = %d\n", __func__, size);
		goto exit;
	}

	retval = synaptics_ts_erase_flash_opt(ts, reflash_data,
			flash_addr, size, resp_reading);
	if (retval < 0) {
		input_err(true, ts->dev, "%s: Fail to erase %s data (addr: 0x%x, size: %d)\n", __func__,
			AREA_ID_STR(block->id), flash_addr, size);
		return retval;
	}

exit:
	input_info(true, ts->dev, "%s: %s area erased\n", __func__, AREA_ID_STR(block->id));

	return 0;
}

/**
 * syna_tcm_update_flash_block()
 *
 * Perform the standard sequence to reflash
 *
 * @param
 *    [ in] tcm_dev:       the device handle
 *    [ in] reflash_data:  data blob for reflash
 *    [ in] block:         target block area to update
 *    [ in] delay_setting: set up the us delay time to wait for the completion of flash access
 *                            for polling,     set a value formatted with [erase us | write us];
 *                            for ATTN-driven, use '0' or 'RESP_IN_ATTN'
 *    [ in] opt_write:     flag to do optimized write
 *
 * @return
 *    on success, 0 or positive value; otherwise, negative value on error.
 */
static int synaptics_ts_update_flash_block_opt(struct synaptics_ts_data *ts,
		struct synaptics_ts_reflash_data_blob *reflash_data,
		struct block_data *block, unsigned int delay_setting, bool opt_write)
{
	int retval;
	unsigned int erase_delay_ms;
	unsigned int wr_blk_delay_ms;

	if (!ts) {
		pr_err("%s%s: Invalid ts device handle\n", SECLOG, __func__);
		return -EINVAL;
	}

	if (!reflash_data) {
		input_err(true, ts->dev, "%s: Invalid reflash data blob\n", __func__);
		return -EINVAL;
	}

	if (!block) {
		input_err(true, ts->dev, "%s: Invalid block data\n", __func__);
		return -EINVAL;
	}

	erase_delay_ms = (delay_setting == FORCE_ATTN_DRIVEN) ? FORCE_ATTN_DRIVEN : ((delay_setting >> 16) & 0xFFFF);
	wr_blk_delay_ms = (delay_setting == FORCE_ATTN_DRIVEN) ? FORCE_ATTN_DRIVEN : (delay_setting & 0xFFFF);

	/* reflash is not needed for the partition */
	retval = synaptics_ts_check_flash_block(ts,
			reflash_data,
			block);
	if (retval < 0) {
		input_err(true, ts->dev, "%s: Invalid %s area\n", __func__, AREA_ID_STR(block->id));
		return retval;
	}

	if (retval == DO_NONE)
		return 0;

	input_err(true, ts->dev, "%s: Prepare to erase %s area\n", __func__, AREA_ID_STR(block->id));

	retval = synaptics_ts_erase_flash_block_opt(ts,
			reflash_data,
			block,
			erase_delay_ms,
			opt_write);
	if (retval < 0) {
		input_err(true, ts->dev, "%s: Fail to erase %s area\n", __func__, AREA_ID_STR(block->id));
		return retval;
	}

	input_err(true, ts->dev, "%s: Prepare to update %s area\n", __func__, AREA_ID_STR(block->id));

	retval = synaptics_ts_write_flash_block_opt(ts,
			reflash_data,
			block,
			wr_blk_delay_ms,
			opt_write);
	if (retval < 0) {
		input_err(true, ts->dev, "%s: Fail to write %s area\n", __func__, AREA_ID_STR(block->id));
		return retval;
	}

	return 0;
}

/**
 * syna_tcm_do_reflash_generic()
 *
 * Implement the generic sequence of fw update in MODE_BOOTLOADER.
 *
 * Typically, it is applied on most of discrete touch controllers
 *
 * @param
 *    [ in] tcm_dev:         the device handle
 *    [ in] reflash_data:    misc. data used for fw update
 *    [ in] type:            the area to update
 *    [ in] delay_setting:   set up the us delay time to wait for the completion of flash access
 *                               for polling,     set a value formatted with [erase ms | write us];
 *                               for ATTN-driven, use '0' or 'RESP_IN_ATTN'
 * @return
 *    on success, 0 or positive value; otherwise, negative value on error.
 */
static int synaptics_ts_do_reflash_generic_opt(struct synaptics_ts_data *ts,
		struct synaptics_ts_reflash_data_blob *reflash_data,
		enum update_area type, unsigned int delay_setting)
{
	int retval = 0;
	struct block_data *block;

	if (!ts) {
		pr_err("%s%s: Invalid ts device handle\n", SECLOG, __func__);
		return -EINVAL;
	}

	if (!reflash_data) {
		input_err(true, ts->dev, "%s: Invalid reflash_data blob\n", __func__);
		return -EINVAL;
	}

	if (ts->dev_mode != SYNAPTICS_TS_MODE_BOOTLOADER) {
		input_err(true, ts->dev, "%s: Incorrect bootloader mode, 0x%02x, expected: 0x%02x\n", __func__,
			ts->dev_mode, SYNAPTICS_TS_MODE_BOOTLOADER);
		return -EINVAL;
	}

	if (type != UPDATE_FIRMWARE_AND_CONFIG) {
		input_err(true, ts->dev, "%s: type is not UPDATE_FIRMWARE_AND_CONFIG\n", __func__);
		return -EINVAL;
	}

	block = &reflash_data->image_info.data[AREA_APP_CODE];

	retval = synaptics_ts_update_flash_block_opt(ts,
			reflash_data,
			block,
			delay_setting,
			ts->is_dualization_ic);
	if (retval < 0) {
		input_err(true, ts->dev, "%s: Fail to update application firmware\n", __func__);
		goto exit;
	}
	block = &reflash_data->image_info.data[AREA_APP_CONFIG];
	retval = synaptics_ts_update_flash_block_opt(ts,
			reflash_data,
			block,
			delay_setting,
			ts->is_dualization_ic);
	if (retval < 0) {
		input_err(true, ts->dev, "%s: Fail to update application config\n", __func__);
		goto exit;
	}

exit:
	return retval;
}

/**
 * syna_tcm_do_fw_update()
 *
 * The entry function to perform fw update upon TouchBoot.
 *
 * @param
 *    [ in] tcm_dev:         the device handle
 *    [ in] image:           binary data to write
 *    [ in] image_size:      size of data array
 *    [ in] delay_setting: set up the us delay time to wait for the completion of flash access
 *                            for polling,     set a value formatted with [erase ms | write us];
 *                            for ATTN-driven, use '0' or 'RESP_IN_ATTN'
 *    [ in] force_reflash:   '1' to do reflash anyway
 *                           '0' to compare ID info before doing reflash.
 *
 * @return
 *    on success, 0 or positive value; otherwise, negative value on error.
 */
int synaptics_ts_do_fw_update_opt(struct synaptics_ts_data *ts,
		const unsigned char *image, unsigned int image_size,
		unsigned int delay_setting, bool force_reflash)
{
	int retval = 0, retry = FW_UPDATE_RETRY_COUNT;
	unsigned int app_status;
	enum update_area type = UPDATE_NONE;
	struct synaptics_ts_reflash_data_blob reflash_data;

	if (!ts) {
		pr_err("%s%s: Invalid ts device handle\n", SECLOG, __func__);
		return -EINVAL;
	}

	if ((!image) || (image_size == 0)) {
		input_err(true, ts->dev, "%s: Invalid image data\n", __func__);
		return -EINVAL;
	}

	input_err(true, ts->dev, "%s: Prepare to do reflash\n", __func__);

	synaptics_ts_buf_init(&reflash_data.out);

	reflash_data.image = image;
	reflash_data.image_size = image_size;
	synaptics_ts_pal_mem_set(&reflash_data.image_info, 0x00,
		sizeof(struct image_info));

	retval = synaptics_ts_parse_fw_image(ts, image, &reflash_data.image_info);
	if (retval < 0) {
		input_err(true, ts->dev, "%s: Fail to parse firmware image\n", __func__);
		synaptics_ts_buf_release(&reflash_data.out);
		return retval;
	}

	input_err(true, ts->dev, "%s: Start of reflash\n", __func__);

	ATOMIC_SET(ts->firmware_flashing, 1);

	app_status = synaptics_ts_pal_le2_to_uint(ts->app_info.status);

	/* to forcedly update the firmware and config
	 *   - flag of 'force_reflash' has been set
	 *   - device stays in bootloader
	 *   - app firmware doesn't run properly
	 */
	if (IS_BOOTLOADER_MODE(ts->dev_mode))
		force_reflash = true;
	if (IS_APP_FW_MODE(ts->dev_mode) && (app_status != APP_STATUS_OK)) {
		input_err(true, ts->dev, "%s: Bad application firmware, dev_mode: 0x%x, status: 0x%x\n",
		__func__, ts->dev_mode, app_status);
		force_reflash = true;
	}

	if (force_reflash) {
		type = UPDATE_FIRMWARE_AND_CONFIG;
		goto reflash;
	}

	type = (enum update_area)synaptics_ts_compare_image_id_info(ts, &reflash_data);

	ts->firmware_update_done = type;

	if (type == UPDATE_NONE)
		goto exit;

reflash:
	do {
		if (retval < 0) {
			input_fail_hist(true, ts->dev, "%s: fw update is failed. retry:%d\n",
					__func__, FW_UPDATE_RETRY_COUNT - retry);
			synaptics_ts_hw_reset(ts);
			retval = synaptics_ts_setup(ts);
			if (retval < 0) {
				input_err(true, ts->dev, "%s: fail to do setup, retry=%d\n",
						__func__, FW_UPDATE_RETRY_COUNT - retry);
				continue;
			}
		}
		synaptics_ts_buf_init(&reflash_data.out);

		/* set up flash access, and enter the bootloader mode */
		retval = synaptics_ts_set_up_flash_access(ts, &reflash_data);
		if (retval < 0) {
			input_err(true, ts->dev,
					"%s: fail to set up flash access, %d\n", __func__, retval);
		} else {
			/* perform the fw update */
			if (ts->dev_mode == SYNAPTICS_TS_MODE_BOOTLOADER) {
				if (ts->is_dualization_ic)
					input_info(true, ts->dev, "%s: has optimized write support, %s\n", __func__, (char *)ts->id_info.part_number);

				retval = synaptics_ts_do_reflash_generic_opt(ts,
					&reflash_data,
					type,
					delay_setting);
				if (retval < 0) {
					input_err(true, ts->dev, "%s: fail to do firmware update, retry=%d\n",
							__func__, FW_UPDATE_RETRY_COUNT - retry);
				} else {
					input_info(true, ts->dev, "%s: succeed to do firmware update\n", __func__);
					if (synaptics_ts_soft_reset(ts) < 0)
						input_err(true, ts->dev, "Fail to do reset\n");
				}
			} else {
				retval = -EINVAL;
				input_err(true, ts->dev, "%s: Incorrect bootloader mode, 0x%02x\n", __func__,
					ts->dev_mode);
			}
		}
	} while ((retval < 0) && (--retry > 0));
exit:
	ATOMIC_SET(ts->firmware_flashing, 0);

	synaptics_ts_buf_release(&reflash_data.out);

	return retval;
}

int synaptics_ts_fw_update_on_probe_opt(struct synaptics_ts_data *ts)
{
	int retval = 0;
	const struct firmware *fw_entry = NULL;
	char fw_path[SYNAPTICS_TS_MAX_FW_PATH];
#ifdef TCLM_CONCEPT
	int restore_cal = 0;
#endif
	unsigned int delay_setting = 0;
	int retry = 3;

	input_info(true, ts->dev, "%s:\n", __func__);

	if (ts->plat_data->bringup == 1) {
		input_info(true, ts->dev, "%s: bringup 1\n", __func__);
		goto exit_fwload;
	}

	if (!ts->plat_data->firmware_name) {
		input_err(true, ts->dev, "%s: firmware name does not declair in dts\n", __func__);
		retval = -ENOENT;
		goto exit_fwload;
	}

	snprintf(fw_path, SYNAPTICS_TS_MAX_FW_PATH, "%s", ts->plat_data->firmware_name);
	input_info(true, ts->dev, "%s: Load firmware : %s\n", __func__, fw_path);

	delay_setting = (ts->fw_erase_delay << 16) | ts->fw_write_block_delay;

	if (delay_setting == 0)
		delay_setting = FORCE_ATTN_DRIVEN;

	if (delay_setting != FORCE_ATTN_DRIVEN)
		disable_irq(ts->irq);

	while (retry--) {
		retval = request_firmware(&fw_entry, fw_path, ts->dev);
		if (retval)
			input_err(true, ts->dev,
					"%s: Firmware image %s not available retry %d\n", __func__,
					fw_path, retry);
		else
			break;
		sec_delay(1000);
	}

	if (retval) {
		retval = 0;
		enable_irq(ts->irq);
		goto exit_fwload;
	}

	retval = synaptics_ts_do_fw_update_opt(ts, fw_entry->data, fw_entry->size, delay_setting, false);
	if (retval < 0) {
		input_err(true, ts->dev, "%s: failed fw update, ret=%d\n", __func__, retval);
		enable_irq(ts->irq);
		goto done;
	}

	retval = synaptics_ts_set_up_app_fw(ts);
	if (retval < 0) {
		input_err(true, ts->dev, "%s: Fail to set up application firmware\n", __func__);
		enable_irq(ts->irq);
		goto done;
	}

	if (delay_setting != FORCE_ATTN_DRIVEN)
		enable_irq(ts->irq);

#ifdef TCLM_CONCEPT
	if (ts->firmware_update_done) {
		retval = synaptics_ts_tclm_read(ts->dev, SEC_TCLM_NVM_ALL_DATA);
		if (retval < 0) {
			input_info(true, ts->dev, "%s: SEC_TCLM_NVM_ALL_DATA read fail", __func__);
			goto done;
		}

		input_info(true, ts->dev, "%s: tune_fix_ver [%04X] afe_base [%04X]\n",
			__func__, ts->tdata->nvdata.tune_fix_ver, ts->tdata->afe_base);

		if ((ts->tdata->tclm_level > TCLM_LEVEL_CLEAR_NV) &&
			((ts->tdata->nvdata.tune_fix_ver == 0xffff)
			|| (ts->tdata->afe_base > ts->tdata->nvdata.tune_fix_ver))) {
			/* tune version up case */
			sec_tclm_root_of_cal(ts->tdata, CALPOSITION_TUNEUP);
			restore_cal = true;
		} else if (ts->tdata->tclm_level == TCLM_LEVEL_CLEAR_NV) {
			/* firmup case */
			sec_tclm_root_of_cal(ts->tdata, CALPOSITION_FIRMUP);
			restore_cal = true;
		} else if ((ts->tdata->nvdata.tune_fix_ver >> 8) == 0x31) {
			sec_tclm_root_of_cal(ts->tdata, CALPOSITION_TUNEUP);
			restore_cal = true; /* temp */
		}

		if (restore_cal) {
			input_info(true, ts->dev, "%s: RUN OFFSET CALIBRATION\n", __func__);
			if (sec_execute_tclm_package(ts->tdata, 0) < 0)
				input_err(true, ts->dev, "%s: sec_execute_tclm_package fail\n", __func__);
		}
	}
#endif

done:
#ifdef TCLM_CONCEPT
	sec_tclm_root_of_cal(ts->tdata, CALPOSITION_NONE);
#endif
	release_firmware(fw_entry);
exit_fwload:
	return retval;
}

static int synaptics_ts_load_fw_from_bin_opt(struct synaptics_ts_data *ts)
{
	int error = 0;
	int restore_cal = 0;
	const struct firmware *fw_entry;
	char fw_path[SEC_TS_MAX_FW_PATH];
	unsigned int delay_setting = 0;

	if (ts->plat_data->bringup == 1) {
		error = -1;
		input_err(true, ts->dev, "%s: can't update for bringup:%d\n",
				__func__, ts->plat_data->bringup);
		return error;
	}

	if (ts->plat_data->firmware_name)
		snprintf(fw_path, SEC_TS_MAX_FW_PATH, "%s", ts->plat_data->firmware_name);
	else
		return 0;


	delay_setting = (ts->fw_erase_delay << 16) | ts->fw_write_block_delay;

	if (delay_setting == 0)
		delay_setting = FORCE_ATTN_DRIVEN;

	if (delay_setting != FORCE_ATTN_DRIVEN)
		disable_irq(ts->irq);

	/* Loading Firmware */
	error = request_firmware(&fw_entry, fw_path, ts->dev);
	if (error) {
		input_err(true, ts->dev, "%s: not exist firmware\n", __func__);
		error = -1;
		if (delay_setting != FORCE_ATTN_DRIVEN)
			enable_irq(ts->irq);
		goto err_request_fw;
	}

#ifdef TCLM_CONCEPT
	sec_tclm_root_of_cal(ts->tdata, CALPOSITION_TESTMODE);
	restore_cal = 1;
#endif
	/* use virtual tclm_control - magic cal 1 */
	error = synaptics_ts_do_fw_update_opt(ts,
			fw_entry->data,
			fw_entry->size,
			delay_setting,
			true);
	if (error < 0) {
		input_err(true, ts->dev, "Fail to do reflash\n");
		restore_cal = 0;
	}

	/* re-initialize the app fw */
	error = synaptics_ts_set_up_app_fw(ts);
	if (error < 0) {
		input_err(true, ts->dev, "Fail to set up app fw after fw update\n");
		release_firmware(fw_entry);
		if (delay_setting != FORCE_ATTN_DRIVEN)
			enable_irq(ts->irq);
		goto err_request_fw;
	}

	if (delay_setting != FORCE_ATTN_DRIVEN)
		enable_irq(ts->irq);

#ifdef TCLM_CONCEPT
	if (restore_cal == 1)
		sec_execute_tclm_package(ts->tdata, 0);
	sec_tclm_root_of_cal(ts->tdata, CALPOSITION_NONE);
#endif

	release_firmware(fw_entry);
err_request_fw:

	return error;
}

static int synaptics_ts_load_fw_opt(struct synaptics_ts_data *ts, int update_type)
{
	int error = 0;
	struct app_config_header *header;
	struct synaptics_ts_reflash_data_blob reflash_data;
	const struct firmware *fw_entry;
	char fw_path[SEC_TS_MAX_FW_PATH];
	bool is_fw_signed = false;
#ifdef SUPPORT_FW_SIGNED
	long spu_ret = 0;
	long ori_size = 0;
#endif
#ifdef TCLM_CONCEPT
	int restore_cal = 0;
#endif
	unsigned int delay_setting = 0;

	delay_setting = (ts->fw_erase_delay << 16) | ts->fw_write_block_delay;

	if (delay_setting == 0)
		delay_setting = FORCE_ATTN_DRIVEN;

	if (delay_setting != FORCE_ATTN_DRIVEN)
		disable_irq(ts->irq);

	switch (update_type) {
	case TSP_SDCARD:
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
		snprintf(fw_path, SEC_TS_MAX_FW_PATH, "%s", TSP_EXTERNAL_FW);
#else
		snprintf(fw_path, SEC_TS_MAX_FW_PATH, "%s", TSP_EXTERNAL_FW_SIGNED);
		is_fw_signed = true;
#endif
		break;
	case TSP_SPU:
	case TSP_VERIFICATION:
		snprintf(fw_path, SEC_TS_MAX_FW_PATH, "%s", TSP_SPU_FW_SIGNED);
		is_fw_signed = true;
		break;
	default:
		goto err_firmware_path;
	}

	error = request_firmware(&fw_entry, fw_path, ts->dev);
	if (error) {
		input_err(true, ts->dev, "%s: firmware is not available %d\n", __func__, error);
		goto err_request_fw;
	}

	synaptics_ts_buf_init(&reflash_data.out);

	reflash_data.image = fw_entry->data;
	reflash_data.image_size = fw_entry->size;
	synaptics_ts_pal_mem_set(&reflash_data.image_info, 0x00,
		sizeof(struct image_info));

	error = synaptics_ts_parse_fw_image(ts, fw_entry->data, &reflash_data.image_info);
	if (error < 0) {
		input_err(true, ts->dev, "%s: Fail to parse firmware image\n", __func__);
		synaptics_ts_buf_release(&reflash_data.out);
		release_firmware(fw_entry);
		goto err_request_fw;
	}

	header = (struct app_config_header *)reflash_data.image_info.data[AREA_APP_CONFIG].data;

#ifdef SUPPORT_FW_SIGNED
	/* If SPU firmware version is lower than IC's version, do not run update routine */
	if (update_type == TSP_VERIFICATION) {
		ori_size = fw_entry->size - SPU_METADATA_SIZE(TSP);
		spu_ret = spu_firmware_signature_verify("TSP", fw_entry->data, fw_entry->size);
		if (spu_ret != ori_size) {
			input_err(true, ts->dev, "%s: signature verify failed, spu_ret:%ld, ori_size:%ld\n",
				__func__, spu_ret, ori_size);
			error = -EPERM;
		}
		synaptics_ts_buf_release(&reflash_data.out);
		release_firmware(fw_entry);
		goto err_request_fw;

	} else if (is_fw_signed) {
		/* digest 32, signature 512 TSP 3 */
		ori_size = fw_entry->size - SPU_METADATA_SIZE(TSP);
		if ((update_type == TSP_SPU) && (ts->plat_data->img_version_of_ic[0] == header->customer_config_id[0] &&
			ts->plat_data->img_version_of_ic[1] == header->customer_config_id[1] &&
			ts->plat_data->img_version_of_ic[2] == header->customer_config_id[2])) {
			if (ts->plat_data->img_version_of_ic[3] >= header->customer_config_id[3]) {
				input_err(true, ts->dev, "%s: img version: %02X%02X%02X%02Xexit\n",
					__func__, ts->plat_data->img_version_of_ic[0], ts->plat_data->img_version_of_ic[1],
					ts->plat_data->img_version_of_ic[2], ts->plat_data->img_version_of_ic[3]);
				error = 0;
				input_info(true, ts->dev, "%s: skip spu\n", __func__);
				goto done;
			} else {
				input_info(true, ts->dev, "%s: run spu\n", __func__);
			}
		} else if ((update_type == TSP_SDCARD) && (ts->plat_data->img_version_of_ic[0] == header->customer_config_id[0] &&
			ts->plat_data->img_version_of_ic[1] == header->customer_config_id[1])) {
			input_info(true, ts->dev, "%s: run sfu\n", __func__);
		} else {
			input_info(true, ts->dev, "%s: not matched product version\n", __func__);
			error = -ENOENT;
			goto done;
		}

		spu_ret = spu_firmware_signature_verify("TSP", fw_entry->data, fw_entry->size);
		if (spu_ret != ori_size) {
			input_err(true, ts->dev, "%s: signature verify failed, spu_ret:%ld, ori_size:%ld\n",
				__func__, spu_ret, ori_size);
			error = -EPERM;
			goto done;
		}
	}
#endif

#ifdef TCLM_CONCEPT
	sec_tclm_root_of_cal(ts->tdata, CALPOSITION_TESTMODE);
	restore_cal = 1;
#endif
	error = synaptics_ts_do_fw_update_opt(ts,
			fw_entry->data,
			fw_entry->size,
			delay_setting,
			true);
	if (error < 0) {
		input_err(true, ts->dev, "Fail to do reflash\n");
		goto done;
	}

	/* re-initialize the app fw */
	error = synaptics_ts_set_up_app_fw(ts);
	if (error < 0) {
		input_err(true, ts->dev, "Fail to set up app fw after fw update\n");
		goto done;
	}

	if (delay_setting != FORCE_ATTN_DRIVEN)
		enable_irq(ts->irq);

#ifdef TCLM_CONCEPT
	sec_execute_tclm_package(ts->tdata, 0);
#endif
done:
	synaptics_ts_buf_release(&reflash_data.out);
#ifdef TCLM_CONCEPT
	sec_tclm_root_of_cal(ts->tdata, CALPOSITION_NONE);
#endif
	release_firmware(fw_entry);
err_request_fw:
err_firmware_path:
	enable_irq(ts->irq);
	return error;

}

int synaptics_ts_fw_update_on_hidden_menu_opt(struct synaptics_ts_data *ts, int update_type)
{
	int retval = SEC_ERROR;

	/* Factory cmd for firmware update
	 * argument represent what is source of firmware like below.
	 *
	 * 0 : [BUILT_IN] Getting firmware which is for user.
	 * 1 : [UMS] Getting firmware from sd card.
	 * 2 : none
	 * 3 : [FFU] Getting firmware from apk.
	 */
	switch (update_type) {
	case TSP_BUILT_IN:
		retval = synaptics_ts_load_fw_from_bin_opt(ts);
		break;

	case TSP_SDCARD:
	case TSP_SPU:
	case TSP_VERIFICATION:
		retval = synaptics_ts_load_fw_opt(ts, update_type);
		break;

	default:
		input_err(true, ts->dev, "%s: Not support command[%d]\n",
				__func__, update_type);
		break;
	}

	synaptics_ts_get_custom_library(ts);
	ts->plat_data->init(ts);

	return retval;
}

MODULE_LICENSE("GPL");
