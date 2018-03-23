/*
 * drivers/misc/nasa_proximity.c
 *
 * Copyright (C) 2015, Levin Calado <levincalado@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/input.h>
#include <linux/hrtimer.h>
#include <asm-generic/cputime.h>
#include <linux/nasa_proximity.h>

int is_screen_on;
char alsps_dev = 0;

#ifdef CONFIG_NASA_PROXIMITY
unsigned nasa_proximity_switch = 1;
#else
unsigned nasa_proximity_switch = 0;
#endif

static unsigned int nasa_proximity_timeout = 600;
static cputime64_t read_time_pre = 0;
static int prev_res = 0;

static int (*sensor_check)(void) = NULL;

int ps_is_close(void) {

	if (!(nasa_proximity_switch))
		return 0;

	if (sensor_check == NULL)
		return 0;

	if (!(is_screen_on)) {
		if (nasa_proximity_timeout) {
			if ((ktime_to_ms(ktime_get()) - read_time_pre) < nasa_proximity_timeout) {
				return prev_res;
			}
			read_time_pre = ktime_to_ms(ktime_get());
		}
		if (nasa_proximity_switch){
			if (sensor_check() == 1) {
				prev_res = 0;
				return 0;
			} else {
				prev_res = 1;
				return 1;
			}
		}
	}

	return 0;
}

static ssize_t nasa_proximity_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", nasa_proximity_switch);
}

static ssize_t nasa_proximity_set(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int val = 0;

	sscanf(buf, "%u\n", &val);

	if ( ( val == 0 ) || ( val == 1 ) )
		nasa_proximity_switch = val;

	return size;
}

static DEVICE_ATTR(enable, (S_IWUSR|S_IRUGO),
		nasa_proximity_show, nasa_proximity_set);

static ssize_t nasa_proximity_timeout_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", nasa_proximity_timeout);
}

static ssize_t nasa_proximity_timeout_set(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int val = 0;

	if (sscanf(buf, "%u\n", &val) == 1) {
		nasa_proximity_timeout = val;
	}

	return size;
}

static DEVICE_ATTR(timeout, (S_IWUSR|S_IRUGO),
		nasa_proximity_timeout_show, nasa_proximity_timeout_set);

static struct attribute *nasa_proximity_attributes[] =
{
	&dev_attr_enable.attr,
	&dev_attr_timeout.attr,
	NULL
};

static struct attribute_group nasa_proximity_group =
{
	.attrs  = nasa_proximity_attributes,
};

#define NASA_PROXIMITY_DECLARED
#ifdef NASA_PROXIMITY_DECLARED
extern struct kobject *nasa_proximity_kobj;
#else
struct kobject *nasa_proximity_kobj;
EXPORT_SYMBOL_GPL(nasa_proximity_kobj);
#endif

static int nasa_proximity_init_sysfs(void) {

	int rc = 0;

	struct kobject *nasa_proximity_kobj;
	nasa_proximity_kobj = kobject_create_and_add("nasa_proximity", NULL);

	rc = sysfs_create_group(nasa_proximity_kobj,
			&nasa_proximity_group);

	if (unlikely(rc < 0))
		pr_err("nasa_proximity: sysfs_create_group failed: %d\n", rc);

	return rc;

}

static int nasa_proximity_init(void) {

	int rc = 0;

	rc = nasa_proximity_init_sysfs();

	sensor_check = epl2182_proximity_check;

	return rc;

}

late_initcall(nasa_proximity_init);
