/*
 * Screen wake timeout
 * Copyright (C) 2014 flar2 <asegaert@gmail.com>
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

#include <linux/module.h>
#include <linux/device.h>
#include <linux/android_alarm.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/sysfs.h>
#include <linux/init.h>
#include <linux/earlysuspend.h>
#include <linux/sweep2wake.h>

#define WAKE_TIMEOUT_MAJOR_VERSION	2
#define WAKE_TIMEOUT_MINOR_VERSION	0
#define WAKEFUNC "wakefunc"

static unsigned int wake_timeout = 0;
static struct alarm wakefunc_rtc;
static bool wakefunc_triggered = false;
void ext_elan_ktf3k_ts_set_power_state(void);

static void wake_presspwr(struct work_struct * wake_presspwr_work) {
	wakefunc_triggered = true;
	ext_elan_ktf3k_ts_set_power_state();
	return;
}
static DECLARE_WORK(wake_presspwr_work, wake_presspwr);

static void wake_pwrtrigger(void) {
	schedule_work(&wake_presspwr_work);
        return;
}

static void wakefunc_rtc_start(void)
{
	ktime_t wakeup_time;
	ktime_t curr_time;

	wakefunc_triggered = false;
	curr_time = alarm_get_elapsed_realtime();
	wakeup_time = ktime_add_us(curr_time,
			(wake_timeout * USEC_PER_MSEC * 60000));
	alarm_start_range(&wakefunc_rtc, wakeup_time,
			wakeup_time);
	pr_debug("%s: Current Time: %ld, Alarm set to: %ld\n",
			WAKEFUNC,
			ktime_to_timeval(curr_time).tv_sec,
			ktime_to_timeval(wakeup_time).tv_sec);
		
	pr_info("%s: Timeout started for %u minutes\n", WAKEFUNC,
			wake_timeout);
}

static void wakefunc_rtc_cancel(void)
{
	int ret;

	wakefunc_triggered = false;
	ret = alarm_cancel(&wakefunc_rtc);
	if (ret)
		pr_info("%s: Timeout canceled\n", WAKEFUNC);
	else
		pr_info("%s: Nothing to cancel\n",
				WAKEFUNC);
}


static void wakefunc_rtc_callback(struct alarm *al)
{
	struct timeval ts;
	ts = ktime_to_timeval(alarm_get_elapsed_realtime());

	wake_pwrtrigger();
	
	pr_debug("%s: Time of alarm expiry: %ld\n", WAKEFUNC,
			ts.tv_sec);
}


//sysfs
static ssize_t show_wake_timeout(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", wake_timeout);
}

static ssize_t store_wake_timeout(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int input;
	int ret;

	ret = sscanf(buf, "%u", &input);

	if (ret != 1) {
		return -EINVAL;
	}

	wake_timeout = input;

	return count;
}

static DEVICE_ATTR(wake_timeout, (S_IWUSR|S_IRUGO),
	show_wake_timeout, store_wake_timeout);

extern struct kobject *android_touch_kobj;

static void wake_timeout_early_suspend(struct early_suspend *h)
{
	if (!pwr_key_pressed && !lid_closed && !wakefunc_triggered && wake_timeout > 0)
		wakefunc_rtc_start();
	
	return; 
}

static void wake_timeout_late_resume(struct early_suspend *h)
{
	wakefunc_rtc_cancel();
	return; 
}

static struct early_suspend wake_timeout_early_suspend_driver = {
	.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 10,
	.suspend = wake_timeout_early_suspend,
	.resume = wake_timeout_late_resume,
};

static int __init wake_timeout_init(void)
{
	int rc;

	pr_info("wake_timeout version %d.%d\n",
		 WAKE_TIMEOUT_MAJOR_VERSION,
		 WAKE_TIMEOUT_MINOR_VERSION);

	alarm_init(&wakefunc_rtc, ANDROID_ALARM_ELAPSED_REALTIME_WAKEUP,
			wakefunc_rtc_callback);

	rc = sysfs_create_file(android_touch_kobj, &dev_attr_wake_timeout.attr);
	if (rc) {
		pr_warn("%s: sysfs_create_file failed for wake_timeout\n", __func__);
	}

	register_early_suspend(&wake_timeout_early_suspend_driver);

	return 0;
}


static void __exit wake_timeout_exit(void)
{

	alarm_cancel(&wakefunc_rtc);

	return;
}

MODULE_AUTHOR("flar2 <asegaert@gmail.com>");
MODULE_DESCRIPTION("'wake_timeout' - Disable screen wake functions after timeout");
MODULE_LICENSE("GPL v2");

module_init(wake_timeout_init);
module_exit(wake_timeout_exit);
