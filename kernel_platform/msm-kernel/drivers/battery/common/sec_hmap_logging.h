/*
 * sec_hmap_logging.h
 * Samsung Mobile Battery Header
 *
 *
 * Copyright (C) 2012 Samsung Electronics, Inc.
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
 
 #ifndef __SEC_HMAP_LOGGING_H
#define __SEC_HMAP_LOGGING_H __FILE__

int sec_hmap_set_logging_control(struct sec_battery_info *battery, int val);
int sec_hmap_get_buf_status(struct sec_battery_info *battery);
int hmap_logging_init(struct sec_battery_info *battery);
int hmap_logging_suspend(struct sec_battery_info *battery);
int hmap_logging_resume(struct sec_battery_info *battery);
int hmap_logging_remove(struct sec_battery_info *battery);

#endif /* __SEC_HMAP_LOGGING_H */