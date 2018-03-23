#ifndef _LINUX_DOUBLETAP2WAKE_H
#define _LINUX_DOUBLETAP2WAKE_H

extern int dt2w_switch;
extern bool dt2w_scr_suspended;

void doubletap2wake_setdev(struct input_dev *);

#endif	/* _LINUX_DOUBLETAP2WAKE_H */