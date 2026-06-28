/*
 *  sec_hmap_logging.c
 *  Samsung Mobile Battery Driver
 *
 *  Copyright (C) 2018 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include "sec_battery.h"
extern void sec_bat_get_battery_info(struct sec_battery_info *battery);

static void sec_hmap_start_logging(struct sec_battery_info *battery)
{
	queue_delayed_work(system_wq, &battery->sec_hmap_logging_work, 0);
	pr_debug("%s: queued logging work\n", __func__);
}

static void sec_hmap_stop_logging(struct sec_battery_info *battery)
{
	cancel_delayed_work_sync(&battery->sec_hmap_logging_work);
	pr_debug("%s: logging stopped\n", __func__);
}

static void sec_hmap_logging_work(struct work_struct *work)
{
    struct sec_battery_info *battery = container_of(work, struct sec_battery_info, sec_hmap_logging_work.work);
    union power_supply_propval value = {0, };
    struct sec_battery_hmap_data *current_reading;
    u64 boottime_ns = 0;

    current_reading = &battery->hmap_buffer[battery->hmap_buffer_index];

    pr_info("%s: index (%d)\n", __func__, battery->hmap_buffer_index);

    // Read Required values
	value.intval = SEC_BATTERY_VOLTAGE_MV;
    psy_do_property(battery->pdata->fuelgauge_name, get, POWER_SUPPLY_PROP_VOLTAGE_NOW, value);
    current_reading->voltage_now = value.intval;

    value.intval = SEC_BATTERY_CURRENT_MA;
    psy_do_property(battery->pdata->fuelgauge_name, get, POWER_SUPPLY_PROP_CURRENT_AVG, value);
    current_reading->current_avg = value.intval;

	current_reading->batt_temp = sec_bat_get_temperature(battery->dev, &battery->pdata->bat_thm_info, battery->temperature,
			battery->pdata->charger_name, battery->pdata->fuelgauge_name, battery->pdata->adc_read_type);

	value.intval = SEC_FUELGAUGE_CAPACITY_TYPE_RAW;
	psy_do_property(battery->pdata->fuelgauge_name, get, POWER_SUPPLY_PROP_CAPACITY, value);
	current_reading->capacity = value.intval / 100;

	psy_do_property(battery->pdata->charger_name, get, POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE, value);
    current_reading->fg_full_voltage = value.intval;

	current_reading->recharging_voltage = battery->pdata->recharge_condition_vcell;
	if (battery->current_event & SEC_BAT_CURRENT_EVENT_SWELLING_MODE) {
		if (battery->current_event & SEC_BAT_CURRENT_EVENT_HIGH_TEMP_SWELLING)
			current_reading->recharging_voltage = battery->pdata->swelling_high_rechg_voltage;
		else if (battery->current_event & SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING_COOL3)
			current_reading->recharging_voltage = battery->pdata->swelling_low_cool3_rechg_voltage;
		else /* cool1 cool2 */
			current_reading->recharging_voltage = battery->pdata->swelling_low_rechg_voltage;
	}

    boottime_ns = ktime_get_boottime_ns();
    current_reading->timestamp = boottime_ns / NSEC_PER_MSEC;
	pr_info("%s: Time_Stamp (ms= %llu)\n", __func__, current_reading->timestamp);

	value.intval = SEC_BATTERY_CAPACITY_QH;
	psy_do_property(battery->pdata->fuelgauge_name, get, POWER_SUPPLY_PROP_ENERGY_NOW, value);
	current_reading->cc_info = value.intval;

    // Update buffer index
    battery->hmap_buffer_index++;
    if (battery->hmap_buffer_index >= HMAP_BUFFER_SIZE) {
		pr_info("%s: buffer full\n", __func__);
        battery->hmap_buffer_full = true;
    } else {
		queue_delayed_work(system_wq, &battery->sec_hmap_logging_work, msecs_to_jiffies(1000));
	}
}

int sec_hmap_set_logging_control(struct sec_battery_info *battery, int val)
{
	if (!battery->hmap_buffer) {
		pr_err("%s: no buffer\n", __func__);
		return -ENODEV;
	}
	if (work_busy(&battery->sec_hmap_logging_work.work)) {
		pr_err("%s: already working\n", __func__);
		return -EBUSY;
	}

	if (val == 1) {
		battery->hmap_buffer_full = false;
		battery->hmap_buffer_index = 0;
		battery->is_ect_enabled = true;
		sec_hmap_start_logging(battery);
	} else if (val == 0) {
		sec_hmap_stop_logging(battery);
		battery->is_ect_enabled = false;
	}
	pr_info("%s: val (%d)\n", __func__, val);

	return 0;
}

int sec_hmap_get_buf_status(struct sec_battery_info *battery)
{
 	if (!battery->hmap_buffer) {
		pr_err("%s: no buffer\n", __func__);
		return -ENODEV;
	}

	pr_debug("%s: hmap_buffer_full (%d)\n", __func__, battery->hmap_buffer_full);
	return battery->hmap_buffer_full;
}

int hmap_logging_init(struct sec_battery_info *battery)
{
	int ret = 0;

	pr_debug("%s: start\n", __func__);
	battery->hmap_buffer = devm_kzalloc(battery->dev,
			sizeof(struct sec_battery_hmap_data) * HMAP_BUFFER_SIZE,
			GFP_KERNEL);
	if (!battery->hmap_buffer) {
		dev_err(battery->dev, "Failed to allocate memory\n");
		ret = -ENOMEM;
		goto err_hmap_buf;
	}
	battery->hmap_buffer_index = 0;
	battery->hmap_buffer_full = false;
	battery->is_ect_enabled = false;

	INIT_DELAYED_WORK(&battery->sec_hmap_logging_work, sec_hmap_logging_work);

	pr_info("%s: done\n", __func__);

err_hmap_buf:
	return ret;
}

int hmap_logging_suspend(struct sec_battery_info *battery) {

	pr_info("%s\n", __func__);
	if (!battery->hmap_buffer) {
		pr_err("%s: no buffer\n", __func__);
		return 0;
	}
	cancel_delayed_work_sync(&battery->sec_hmap_logging_work);
	pr_debug("%s: done\n", __func__);
	return 0;
}

int hmap_logging_resume(struct sec_battery_info *battery) {

	if (!battery->hmap_buffer) {
		pr_err("%s: no buffer\n", __func__);
		return 0;
	}
	pr_info("%s: is_ect_enabled(%d), hmap_buffer_full(%d)\n",
			__func__, battery->is_ect_enabled, battery->hmap_buffer_full);

	if(battery->is_ect_enabled && !battery->hmap_buffer_full) {
		sec_hmap_start_logging(battery);
	}
	pr_debug("%s: done\n", __func__);
	return 0;
}

int hmap_logging_remove(struct sec_battery_info *battery) {

	pr_debug("%s: start\n", __func__);
	if (!battery->hmap_buffer) {
		pr_err("%s: no buffer\n", __func__);
		return 0;
	}
	cancel_delayed_work_sync(&battery->sec_hmap_logging_work);

	pr_debug("%s: done\n", __func__);
	return 0;
}

