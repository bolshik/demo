#ifndef _LINUX_SWEEP2WAKE_H
#define _LINUX_SWEEP2WAKE_H

extern int s2w_switch, s2w_s2sonly;
extern bool s2w_scr_suspended;

void sweep2wake_setdev(struct input_dev *);

#endif	/* _LINUX_SWEEP2WAKE_H */