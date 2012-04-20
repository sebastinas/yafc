#ifndef _mingw32_h_included
#define _mingw32_h_included

/**
* mingw stuff - will eventually get it's own header
**/
#define mkdir(x, y) _mkdir((x))

typedef int uid_t;
typedef int gid_t;
/** END mingw stuff **/


#include <winsock2.h>

#define IS_WINDOWS 1;


#endif		// end guard
