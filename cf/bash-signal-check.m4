dnl Check type of signal routines (posix, 4.2bsd, 4.1bsd or v7)
AC_DEFUN(BASH_SIGNAL_CHECK,
[AC_REQUIRE([AC_TYPE_SIGNAL])
AC_MSG_CHECKING(for type of signal functions)
AC_CACHE_VAL(bash_cv_signal_vintage,
[
  AC_TRY_LINK([#include <signal.h>],[
    sigset_t ss;
    struct sigaction sa;
    sigemptyset(&ss); sigsuspend(&ss);
    sigaction(SIGINT, &sa, (struct sigaction *) 0);
    sigprocmask(SIG_BLOCK, &ss, (sigset_t *) 0);
  ], bash_cv_signal_vintage=posix,
  [
    AC_TRY_LINK([#include <signal.h>], [
	int mask = sigmask(SIGINT);
	sigsetmask(mask); sigblock(mask); sigpause(mask);
    ], bash_cv_signal_vintage=4.2bsd,
    [
      AC_TRY_LINK([
	#include <signal.h>
	RETSIGTYPE foo() { }], [
		int mask = sigmask(SIGINT);
		sigset(SIGINT, foo); sigrelse(SIGINT);
		sighold(SIGINT); sigpause(SIGINT);
        ], bash_cv_signal_vintage=svr3, bash_cv_signal_vintage=v7
    )]
  )]
)
])
AC_MSG_RESULT($bash_cv_signal_vintage)
if test "$bash_cv_signal_vintage" = posix; then
AC_DEFINE([HAVE_POSIX_SIGNALS], [], [define if you have POSIX-type signals])
elif test "$bash_cv_signal_vintage" = "4.2bsd"; then
AC_DEFINE([HAVE_BSD_SIGNALS], [], [define if you have BSD-type signals])
elif test "$bash_cv_signal_vintage" = svr3; then
AC_DEFINE([HAVE_USG_SIGHOLD], [], [define if you have usg sighold])
fi
])
