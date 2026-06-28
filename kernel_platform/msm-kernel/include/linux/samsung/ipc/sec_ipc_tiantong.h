/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * COPYRIGHT(C) 2015-2025 Samsung Electronics Co., Ltd. All Right Reserved.
 */

#ifndef __SEC_IPC_TIANTONG_H__
#define __SEC_IPC_TIANTONG_H__

void tiantong_set_ap2cp_wakeup(int value);
int tiantong_get_ap2cp_wakeup(void);
int tiantong_get_ap2cp_status(void);
int tiantong_get_cp2ap_wakeup(void);
int tiantong_get_cp2ap_status(void);
int tiantong_active(void);

#endif /* __SEC_IPC_TIANTONG_H__ */
