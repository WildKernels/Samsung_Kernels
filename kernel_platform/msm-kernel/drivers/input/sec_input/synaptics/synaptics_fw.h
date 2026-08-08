/* SPDX-License-Identifier: GPL-2.0
 * Copyright (C) 2022 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _LINUX_SYNAPTICS_TS_FW_H_
#define _LINUX_SYNAPTICS_TS_FW_H_

#include "synaptics_dev.h"
#include "synaptics_reg.h"

#if IS_ENABLED(CONFIG_SPU_VERIFY)
#define SUPPORT_FW_SIGNED
#endif

#ifdef SUPPORT_FW_SIGNED
#include <linux/spu-verify.h>
#endif

enum {
	TSP_BUILT_IN = 0,
	TSP_SDCARD,
	TSP_SIGNED_SDCARD,
	TSP_SPU,
	TSP_VERIFICATION,
};

#define FW_UPDATE_RETRY_COUNT 3
#define FLASH_READ_DELAY_MS (10)
#define FLASH_WRITE_DELAY_MS (20)
#define FLASH_ERASE_DELAY_MS (500)
#define BOOT_CONFIG_SIZE 8
#define BOOT_CONFIG_SLOTS 16
#define DO_NONE (0)
#define DO_UPDATE (1)

int synaptics_ts_identify(struct synaptics_ts_data *ts, struct synaptics_ts_identification_info *id_info);
int synaptics_ts_set_up_flash_access(struct synaptics_ts_data *ts, struct synaptics_ts_reflash_data_blob *reflash_data);
int synaptics_ts_run_bootloader_fw(struct synaptics_ts_data *ts);
int synaptics_ts_run_application_fw(struct synaptics_ts_data *ts);
int synaptics_ts_switch_fw_mode(struct synaptics_ts_data *ts, unsigned char mode);
int synaptics_ts_compare_image_id_info(struct synaptics_ts_data *ts, struct synaptics_ts_reflash_data_blob *reflash_data);
inline char *synaptics_ts_get_flash_area_string(enum flash_area area);
int synaptics_ts_save_flash_block_data(struct synaptics_ts_data *ts, struct image_info *image_info,
	enum flash_area area, const unsigned char *content,
	unsigned int offset, unsigned int size, unsigned int checksum);
enum flash_area synaptics_ts_get_flash_area_id(char *str);
inline int synaptics_ts_parse_fw_image(struct synaptics_ts_data *ts, const unsigned char *image,
	struct image_info *image_info);
int synaptics_ts_check_flash_boot_config(struct synaptics_ts_data *ts, struct block_data *boot_config,
	struct synaptics_ts_boot_info *boot_info, unsigned int block_size);
int synaptics_ts_check_flash_app_config(struct synaptics_ts_data *ts, struct block_data *app_config,
	struct synaptics_ts_application_info *app_info, unsigned int block_size);
int synaptics_ts_check_flash_disp_config(struct synaptics_ts_data *ts, struct block_data *disp_config,
	struct synaptics_ts_boot_info *boot_info, unsigned int block_size);
int synaptics_ts_check_flash_app_code(struct synaptics_ts_data *ts, struct block_data *app_code);
int synaptics_ts_check_flash_openshort(struct synaptics_ts_data *ts, struct block_data *open_short);
int synaptics_ts_check_flash_app_prod_test(struct synaptics_ts_data *ts, struct block_data *prod_test);
int synaptics_ts_check_flash_ppdt(struct synaptics_ts_data *ts, struct block_data *ppdt);
int synaptics_ts_reflash_send_command(struct synaptics_ts_data *ts,
	unsigned char command, unsigned char *payload,
	unsigned int payload_len, unsigned int delay_ms_resp);
int synaptics_ts_check_flash_block(struct synaptics_ts_data *ts,
	struct synaptics_ts_reflash_data_blob *reflash_data,
	struct block_data *block);
#endif  /* _LINUX_SYNAPTICS_TS_FW_H_ */