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
#include <ws2tcpip.h>

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

// I don't think these will ever work
#define symlink(a, b) 0
#define fork() 0
#define chown(a, b, c) 0

// Simulate a non-root user
#define getuid() 1

// These probably could be defined properly later on
#define sleep(a) 0
#define S_ISLNK(a) 0
#define S_ISSOCK(a) 0

// SIGALRM is used a fair bit - this might be required!
#define alarm(a) 0

// This is actually obsolete
#define herror(a) 0


#endif		// end guard
