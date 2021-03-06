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
#include <linux/input/sweep2wake.h>

#define WAKE_HOOKS_DEFINED

#ifndef WAKE_HOOKS_DEFINED
#ifndef CONFIG_HAS_EARLYSUSPEND
#include <linux/lcd_notify.h>
#else
#include <linux/earlysuspend.h>
#endif
#endif
/*
#ifdef CONFIG_NASA_PROXIMITY
#include <linux/nasa_proximity.h>
#endif
*/
/* uncomment since no touchscreen defines android touch, do that here */
//#define ANDROID_TOUCH_DECLARED

#define DRIVER_VERSION "1.5"
#define LOGTAG "[sweep2wake]: "

MODULE_VERSION(DRIVER_VERSION);
MODULE_LICENSE("GPLv2");

/* Tuneables */
#define S2W_DEBUG             0
#define S2W_DEFAULT           1
#define S2W_S2SONLY_DEFAULT   1
#define S2W_PWRKEY_DUR       60

#define S2W_Y_MAX               854 // bottom border
#define S2W_Y_MIN               0 // upper border
#define S2W_X_MAX               480 // right border
#define S2W_X_MIN               0 // left border
#define S2W_X_B1                60 // left border trigger
#define S2W_X_B2                420 // right border trigger

/* Resources */
int s2w_switch = S2W_DEFAULT, s2w_s2sonly = S2W_S2SONLY_DEFAULT;
static int touch_x = 0, touch_y = 0;
static bool touch_x_called = false, touch_y_called = false;
static bool exec_count = true;
bool s2w_scr_suspended = false;
static bool scr_on_touch = false, barrier[2] = {false, false};
#ifndef WAKE_HOOKS_DEFINED
#ifndef CONFIG_HAS_EARLYSUSPEND
static struct notifier_block s2w_lcd_notif;
#endif
#endif
static struct input_dev * sweep2wake_pwrdev;
static DEFINE_MUTEX(pwrkeyworklock);
static struct workqueue_struct *s2w_input_wq;
static struct work_struct s2w_input_work;

/* PowerKey setter */
void sweep2wake_setdev(struct input_dev * input_device) {
	sweep2wake_pwrdev = input_device;
	printk(LOGTAG"set sweep2wake_pwrdev: %s\n", sweep2wake_pwrdev->name);
}

/* Read cmdline for s2w */
static int __init read_s2w_cmdline(char *s2w)
{
	if (strcmp(s2w, "1") == 0) {
		pr_info("[cmdline_s2w]: Sweep2Wake enabled. | s2w='%s'\n", s2w);
		s2w_switch = 1;
	} else if (strcmp(s2w, "0") == 0) {
		pr_info("[cmdline_s2w]: Sweep2Wake disabled. | s2w='%s'\n", s2w);
		s2w_switch = 0;
	} else {
		pr_info("[cmdline_s2w]: No valid input found. Going with default: | s2w='%u'\n", s2w_switch);
	}
	return 1;
}
__setup("s2w=", read_s2w_cmdline);

/* PowerKey work func */
static void sweep2wake_presspwr(struct work_struct * sweep2wake_presspwr_work) {
	if (!mutex_trylock(&pwrkeyworklock))
		return;
	input_event(sweep2wake_pwrdev, EV_KEY, KEY_POWER, 1);
	input_event(sweep2wake_pwrdev, EV_SYN, 0, 0);
	msleep(S2W_PWRKEY_DUR);
	input_event(sweep2wake_pwrdev, EV_KEY, KEY_POWER, 0);
	input_event(sweep2wake_pwrdev, EV_SYN, 0, 0);
	msleep(S2W_PWRKEY_DUR);
	mutex_unlock(&pwrkeyworklock);
	return;
}
static DECLARE_WORK(sweep2wake_presspwr_work, sweep2wake_presspwr);

/* PowerKey trigger */
static void sweep2wake_pwrtrigger(void) {
	schedule_work(&sweep2wake_presspwr_work);
	return;
}

/* reset on finger release */
static void sweep2wake_reset(void) {
	exec_count = true;
	barrier[0] = false;
	barrier[1] = false;
	scr_on_touch = false;
}

/* Sweep2wake main function */
static void detect_sweep2wake(int x, int y, bool st)
{
	int prevx = 0, nextx = 0;
	bool single_touch = st;
#if S2W_DEBUG
	pr_info(LOGTAG"x,y(%4d,%4d) single:%s\n",
		x, y, (single_touch) ? "true" : "false");
#endif
	//left->right
	if ((single_touch) && (s2w_scr_suspended == true) && (s2w_switch > 0 && !s2w_s2sonly)) {
		prevx = S2W_X_MIN;
		nextx = S2W_X_B1;
		if ((barrier[0] == true) ||
		   ((x > prevx) &&
		    (x < nextx) &&
		    (y > S2W_Y_MIN) &&
		    (y < S2W_Y_MAX))) {
			prevx = nextx;
			nextx = S2W_X_B2;
			barrier[0] = true;
			if ((barrier[1] == true) ||
			   ((x > prevx) &&
			    (x < nextx) &&
			    (y > S2W_Y_MIN) &&
			    (y < S2W_Y_MAX))) {
				prevx = nextx;
				barrier[1] = true;
				if ((x > prevx) &&
				    (y > S2W_Y_MIN) &&
				    (y < S2W_Y_MAX)) {
						if (exec_count) {
							pr_info(LOGTAG"ON\n");
							sweep2wake_pwrtrigger();
							exec_count = false;
					}
				}
			}
		}
	//right->left
	} else if ((single_touch) && (s2w_scr_suspended == false) && (s2w_switch > 0)) {
		scr_on_touch=true;
		prevx = (S2W_X_MAX);
		nextx = S2W_X_B2;
		if ((barrier[0] == true) ||
		   ((x < prevx) &&
		    (x > nextx) &&
		    (y > S2W_Y_MIN) &&
		    (y < S2W_Y_MAX))) {
			prevx = nextx;
			nextx = S2W_X_B1;
			barrier[0] = true;
			if ((barrier[1] == true) ||
			   ((x < prevx) &&
			    (x > nextx) &&
			    (y > S2W_Y_MIN) &&
			    (y < S2W_Y_MAX))) {
				prevx = nextx;
				barrier[1] = true;
				if ((x < prevx) &&
				    (y > S2W_Y_MIN) &&
				    (y < S2W_Y_MAX)) {
						if (exec_count) {
							pr_info(LOGTAG"OFF\n");
							sweep2wake_pwrtrigger();
							exec_count = false;
					}
				}
			}
		}
	}
}

static void s2w_input_callback(struct work_struct *unused) {
/*
	#ifdef CONFIG_NASA_PROXIMITY
	if (ps_is_close()){
		return;
	}
	else
	#endif
*/
	detect_sweep2wake(touch_x, touch_y, true);

	return;
}

static void s2w_input_event(struct input_handle *handle, unsigned int type,
				unsigned int code, int value) {
#if S2W_DEBUG
	pr_info("sweep2wake: code: %s|%u, val: %i\n",
		((code==ABS_MT_POSITION_X) ? "X" :
		(code==ABS_MT_POSITION_Y) ? "Y" :
		((code==ABS_MT_TRACKING_ID)||
			(code==330)) ? "ID" : "undef"), code, value);
#endif
	if (code == ABS_MT_SLOT) {
		sweep2wake_reset();
		return;
	}

	/*
	 * '330'? Many touch panels are 'broken' in the sense of not following the
	 * multi-touch protocol given in Documentation/input/multi-touch-protocol.txt.
	 * According to the docs, touch panels using the type B protocol must send in
	 * a ABS_MT_TRACKING_ID event after lifting the contact in the first slot.
	 * This should in the flow of events, help us reset the sweep2wake variables
	 * and proceed as per the algorithm.
	 *
	 * This however is not the case with various touch panel drivers, and hence
	 * there is no reliable way of tracking ABS_MT_TRACKING_ID on such panels.
	 * Some of the panels however do track the lifting of contact, but with a
	 * different event code, and a different event value.
	 *
	 * So, add checks for those event codes and values to keep the algo flow.
	 *
	 * synaptics_s3203 => code: 330; val: 0
	 *
	 * Note however that this is not possible with panels like the CYTTSP3 panel
	 * where there are no such events being reported for the lifting of contacts
	 * though i2c data has a ABS_MT_TRACKING_ID or equivalent event variable
	 * present. In such a case, make sure the sweep2wake_reset() function is
	 * publicly available for external calls.
	 *
	 */
	if ((code == ABS_MT_TRACKING_ID && value == -1) ||
		(code == 330 && value == 0)) {
		sweep2wake_reset();
		return;
	}

	if (code == ABS_MT_POSITION_X) {
		touch_x = value;
		touch_x_called = true;
	}

	if (code == ABS_MT_POSITION_Y) {
		touch_y = value;
		touch_y_called = true;
	}

	if (touch_x_called && touch_y_called) {
		touch_x_called = false;
		touch_y_called = false;
		queue_work_on(0, s2w_input_wq, &s2w_input_work);
	}
}

static int input_dev_filter(struct input_dev *dev) {
	if (strstr(dev->name, "touch")||
			strstr(dev->name, "mtk-tpd")) {
		return 0;
	} else {
		return 1;
	}
}

static int s2w_input_connect(struct input_handler *handler,
				struct input_dev *dev, const struct input_device_id *id) {
	struct input_handle *handle;
	int error;

	if (input_dev_filter(dev))
		return -ENODEV;

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "s2w";

	error = input_register_handle(handle);
	if (error)
		goto err2;

	error = input_open_device(handle);
	if (error)
		goto err1;

	return 0;
err1:
	input_unregister_handle(handle);
err2:
	kfree(handle);
	return error;
}

static void s2w_input_disconnect(struct input_handle *handle) {
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id s2w_ids[] = {
	{ .driver_info = 1 },
	{ },
};

static struct input_handler s2w_input_handler = {
	.event		= s2w_input_event,
	.connect	= s2w_input_connect,
	.disconnect	= s2w_input_disconnect,
	.name		= "s2w_inputreq",
	.id_table	= s2w_ids,
};

#ifndef WAKE_HOOKS_DEFINED
#ifndef CONFIG_HAS_EARLYSUSPEND
static int lcd_notifier_callback(struct notifier_block *this,
				unsigned long event, void *data)
{
	switch (event) {
	case LCD_EVENT_ON_END:
		s2w_scr_suspended = false;
		break;
	case LCD_EVENT_OFF_END:
		s2w_scr_suspended = true;
		break;
	default:
		break;
	}

	return 0;
}
#else
static void s2w_early_suspend(struct early_suspend *h) {
	s2w_scr_suspended = true;
}

static void s2w_late_resume(struct early_suspend *h) {
	s2w_scr_suspended = false;
}

static struct early_suspend s2w_early_suspend_handler = {
	.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN,
	.suspend = s2w_early_suspend,
	.resume = s2w_late_resume,
};
#endif
#endif

/*
 * SYSFS stuff below here
 */
static ssize_t s2w_sweep2wake_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	size_t count = 0;

	count += sprintf(buf, "%d\n", s2w_switch);

	return count;
}

static ssize_t s2w_sweep2wake_dump(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	if (buf[1] == '\n') {
		if (buf[0] == '0') {
			s2w_switch = 0;
		} else if (buf[0] == '1') {
			s2w_switch = 1;
		}
	}

	return count;
}

static DEVICE_ATTR(sweep2wake, (S_IWUSR|S_IRUGO),
	s2w_sweep2wake_show, s2w_sweep2wake_dump);

static ssize_t s2w_s2w_s2sonly_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	size_t count = 0;

	count += sprintf(buf, "%d\n", s2w_s2sonly);

	return count;
}

static ssize_t s2w_s2w_s2sonly_dump(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	if (buf[1] == '\n') {
		if (buf[0] == '0') {
			s2w_s2sonly = 0;
		} else if (buf[0] == '1') {
			s2w_s2sonly = 1;
		}
	}

	return count;
}

static DEVICE_ATTR(s2w_s2sonly, (S_IWUSR|S_IRUGO),
	s2w_s2w_s2sonly_show, s2w_s2w_s2sonly_dump);

static ssize_t s2w_version_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	size_t count = 0;

	count += sprintf(buf, "%s\n", DRIVER_VERSION);

	return count;
}

static ssize_t s2w_version_dump(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static DEVICE_ATTR(sweep2wake_version, (S_IWUSR|S_IRUGO),
	s2w_version_show, s2w_version_dump);

/*
 * INIT / EXIT stuff below here
 */
#ifdef ANDROID_TOUCH_DECLARED
extern struct kobject *nasa_kobj;
#else
struct kobject *nasa_kobj;
EXPORT_SYMBOL_GPL(nasa_kobj);
#endif
static int __init sweep2wake_init(void)
{
	int rc = 0;

	s2w_input_wq = create_workqueue("s2wiwq");
	if (!s2w_input_wq) {
		pr_err("%s: Failed to create s2wiwq workqueue\n", __func__);
		return -EFAULT;
	}
	INIT_WORK(&s2w_input_work, s2w_input_callback);
	rc = input_register_handler(&s2w_input_handler);
	if (rc)
		pr_err("%s: Failed to register s2w_input_handler\n", __func__);

#ifndef WAKE_HOOKS_DEFINED
#ifndef CONFIG_HAS_EARLYSUSPEND
	s2w_lcd_notif.notifier_call = lcd_notifier_callback;
	if (lcd_register_client(&s2w_lcd_notif) != 0) {
		pr_err("%s: Failed to register lcd callback\n", __func__);
	}
#else
	register_early_suspend(&s2w_early_suspend_handler);
#endif
#endif

#ifndef ANDROID_TOUCH_DECLARED
	nasa_kobj = kobject_create_and_add("nasa", NULL) ;
	if (nasa_kobj == NULL) {
		pr_warn("%s: nasa_kobj create_and_add failed\n", __func__);
	}
#endif
	rc = sysfs_create_file(nasa_kobj, &dev_attr_sweep2wake.attr);
	if (rc) {
		pr_warn("%s: sysfs_create_file failed for sweep2wake\n", __func__);
	}
	rc = sysfs_create_file(nasa_kobj, &dev_attr_s2w_s2sonly.attr);
	if (rc) {
		pr_warn("%s: sysfs_create_file failed for s2w_s2sonly\n", __func__);
	}
	rc = sysfs_create_file(nasa_kobj, &dev_attr_sweep2wake_version.attr);
	if (rc) {
		pr_warn("%s: sysfs_create_file failed for sweep2wake_version\n", __func__);
	}

	return 0;
}

static void __exit sweep2wake_exit(void)
{
#ifndef ANDROID_TOUCH_DECLARED
	kobject_del(nasa_kobj);
#endif
#ifndef WAKE_HOOKS_DEFINED
#ifndef CONFIG_HAS_EARLYSUSPEND
	lcd_unregister_client(&s2w_lcd_notif);
#endif
#endif
	input_unregister_handler(&s2w_input_handler);
	destroy_workqueue(s2w_input_wq);
	return;
}

module_init(sweep2wake_init);
module_exit(sweep2wake_exit);

MODULE_DESCRIPTION("Sweep2wake");
MODULE_LICENSE("GPLv2");