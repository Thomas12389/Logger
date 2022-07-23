
#ifndef __TIME_OPT_H__
#define __TIME_OPT_H__

#define	MAXLEN_TIMEZONE		30
#define DATE_TIME_BYTE      7

extern int get_timezone(char *pTimezone);
extern int set_timezone(const char *pTimezone, const int len);
extern int get_systime(struct tm *get_tm);
extern int set_systime(struct tm set_tm);

#endif