/**
* mingw stuff - will eventually get it's own header
**/


#ifndef _mingw32_h_included
#define _mingw32_h_included

#define IS_WINDOWS 1;

// Win mkdir only takes on arg
#define mkdir(x, y) _mkdir((x))

// User and group id
typedef int uid_t;
typedef int gid_t;

// Need sockets
#include <winsock2.h>

// No such thing as signals
#define SIGALRM 0
#define SIGHUP 0
#define SIGPIPE 0
#define SIGTSTP 0
#define SIGSTOP 0
#define SIGCONT 0
#define SIGQUIT 0
#define SIGTSTP 0

// These don't appear to be in mingw32 stat.h
#define S_ISUID  0004000
#define S_ISGID  0002000
#define S_ISVTX  0001000

// Or these
#define S_IRGRP 00040
#define S_IWGRP 00020
#define S_IXGRP 00010
#define S_IROTH 00004
#define S_IWOTH 00002
#define S_IXOTH 00001

// mingw32 doesn't seem to include this either
typedef uint32_t socklen_t;

// Just nuke some stuff
#define symlink(a, b) 0
#define sleep(a) 0
#define fork() 0

#endif		// end guard


