#ifndef _9DE_SRV9P_H_
#define _9DE_SRV9P_H_

/* minimal shell control plane */
void srv9pstart(void);
void srv9ppostevent(char *fmt, ...);

/* current state knobs */
char* srv9preset(void);
char* srv9panel(void);

#endif
