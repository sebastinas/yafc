AC_DEFUN(BASH_FUNC_POSIX_SETJMP,
[AC_REQUIRE([BASH_SIGNAL_CHECK])
AC_MSG_CHECKING(for presence of POSIX-style sigsetjmp/siglongjmp)
AC_CACHE_VAL(bash_cv_func_sigsetjmp,
[AC_TRY_RUN([
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>
#include <signal.h>
#include <setjmp.h>

main()
{
#if !defined (_POSIX_VERSION) || !defined (HAVE_POSIX_SIGNALS)
exit (1);
#else

int code;
sigset_t set, oset;
sigjmp_buf xx;

/* get the mask */
sigemptyset(&set);
sigemptyset(&oset);
sigprocmask(SIG_BLOCK, (sigset_t *)NULL, &set);
sigprocmask(SIG_BLOCK, (sigset_t *)NULL, &oset);

/* save it */
code = sigsetjmp(xx, 1);
if (code)
  exit(0);	/* could get sigmask and compare to oset here. */

/* change it */
sigaddset(&set, SIGINT);
sigprocmask(SIG_BLOCK, &set, (sigset_t *)NULL);

/* and siglongjmp */
siglongjmp(xx, 10);
exit(1);
#endif
}], bash_cv_func_sigsetjmp=present, bash_cv_func_sigsetjmp=missing,
    [AC_MSG_ERROR(cannot check for sigsetjmp/siglongjmp if cross-compiling -- defaulting to missing)
     bash_cv_func_sigsetjmp=missing]
)])
AC_MSG_RESULT($bash_cv_func_sigsetjmp)
if test $bash_cv_func_sigsetjmp = present; then
AC_DEFINE([HAVE_POSIX_SIGSETJMP], [], [define if you have POSIX sigsetjmp function])
fi
])
