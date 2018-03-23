#ifndef _LINUX_NASA_PROXIMITY_H
#define _LINUX_NASA_PROXIMITY_H

extern int is_screen_on;
extern char alsps_dev;

int epl2182_proximity_check(void);
int ps_is_close(void);

#endif //_LINUX_NASA_PROXIMITY_H