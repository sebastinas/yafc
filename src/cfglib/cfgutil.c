#include <string.h>
#include <stdlib.h>
#include "cfglib.h"

int cfglib_str2bool(const char *e)
{
	if(strcasecmp(e, "on") == 0) return 1;
	else if(strcasecmp(e, "true") == 0) return 1;
	else if(strcasecmp(e, "yes") == 0) return 1;
	else if(strcmp(e, "1") == 0) return 1;
	else if(strcasecmp(e, "off") == 0) return 0;
	else if(strcasecmp(e, "false") == 0) return 0;
	else if(strcasecmp(e, "no") == 0) return 0;
	else if(strcmp(e, "0") == 0) return 0;

	return -1;
}

/* removes character, given in TRIM, at the end of STR
 * if REPEAT is non-zero, removes all matching characters, not only the first
 */
void cfglib_trim_end(char *str, char *trim, int repeat)
{
	char *e = strrchr(str, 0);

	do {
		if(e && e>str && strchr(trim, e[-1]) != 0)
			*(--e) = 0;
		else
			break;
	} while(repeat);
}
