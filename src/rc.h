#ifndef _rc_h_
#define _rc_h_

int parse_rc(const char *file, bool warn);
url_t *get_autologin_url(const char *host);

#endif
