dnl -*- autoconf -*-

dnl not used, not checked

AC_DEFUN(YAFC_CHECK_SSL,
[
  YAFC_ARG_WITH(ssl, yes, SSL)

  if test "$yafc_with_ssl" = "yes"; then
   dnl SSL configure checks stolen from curl
   AC_CHECK_LIB(crypto, CRYPTO_lock)
   if test "$ac_cv_lib_crypto_CRYPTO_lock" = "yes"; then
     AC_CHECK_LIB(ssl, SSL_connect)
     if test "$ac_cv_lib_ssl_SSL_connect" != "yes"; then
       dnl we didn't find the SSL lib, try the RSAglue/rsaref stuff
       AC_MSG_CHECKING(for ssl with RSAglue/rsaref libs in use);
       OLIBS=$LIBS
       LIBS="$LIBS -lRSAglue -lrsaref"
       AC_CHECK_LIB(ssl, SSL_connect)
       if test "$ac_cv_lib_ssl_SSL_connect" != "yes"; then
         dnl still no SSL_connect
         AC_MSG_RESULT(no)
         LIBS=$OLIBS
       else
         AC_MSG_RESULT(yes)
       fi
     fi

     if test "$ac_cv_lib_ssl_SSL_connect" = "yes"; then
       AC_DEFINE(USE_SSL)
       LIBOBJS="$LIBOBJS security.o ssl.o"
       AC_CHECK_HEADERS(openssl/ssl.h)
     fi
   fi
 fi
])
