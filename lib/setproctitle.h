#ifndef __SETPROCTITLE_H
#define __SETPROCTITLE_H

/* Call this from main. */
void initsetproctitle(int argc, char **argv, char **envp);

void setproctitle(const char *fmt, ...);

#endif
