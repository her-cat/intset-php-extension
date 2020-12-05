dnl $Id$
dnl config.m4 for extension intset

dnl Comments in this file start with the string 'dnl'.
dnl Remove where necessary. This file will not work
dnl without editing.

dnl If your extension references something external, use with:

dnl PHP_ARG_WITH(intset, for intset support,
dnl Make sure that the comment is aligned:
dnl [  --with-intset             Include intset support])

dnl Otherwise use enable:

PHP_ARG_ENABLE(intset, whether to enable intset support,
Make sure that the comment is aligned:
[  --enable-intset           Enable intset support])

if test "$PHP_INTSET" != "no"; then
  dnl Write more examples of tests here...

  dnl # --with-intset -> check with-path
  dnl SEARCH_PATH="/usr/local /usr"     # you might want to change this
  dnl SEARCH_FOR="/include/intset.h"  # you most likely want to change this
  dnl if test -r $PHP_INTSET/$SEARCH_FOR; then # path given as parameter
  dnl   INTSET_DIR=$PHP_INTSET
  dnl else # search default path list
  dnl   AC_MSG_CHECKING([for intset files in default path])
  dnl   for i in $SEARCH_PATH ; do
  dnl     if test -r $i/$SEARCH_FOR; then
  dnl       INTSET_DIR=$i
  dnl       AC_MSG_RESULT(found in $i)
  dnl     fi
  dnl   done
  dnl fi
  dnl
  dnl if test -z "$INTSET_DIR"; then
  dnl   AC_MSG_RESULT([not found])
  dnl   AC_MSG_ERROR([Please reinstall the intset distribution])
  dnl fi

  dnl # --with-intset -> add include path
  dnl PHP_ADD_INCLUDE($INTSET_DIR/include)

  dnl # --with-intset -> check for lib and symbol presence
  dnl LIBNAME=intset # you may want to change this
  dnl LIBSYMBOL=intset # you most likely want to change this 

  dnl PHP_CHECK_LIBRARY($LIBNAME,$LIBSYMBOL,
  dnl [
  dnl   PHP_ADD_LIBRARY_WITH_PATH($LIBNAME, $INTSET_DIR/$PHP_LIBDIR, INTSET_SHARED_LIBADD)
  dnl   AC_DEFINE(HAVE_INTSETLIB,1,[ ])
  dnl ],[
  dnl   AC_MSG_ERROR([wrong intset lib version or lib not found])
  dnl ],[
  dnl   -L$INTSET_DIR/$PHP_LIBDIR -lm
  dnl ])
  dnl
  dnl PHP_SUBST(INTSET_SHARED_LIBADD)

  PHP_NEW_EXTENSION(intset, intset.c, $ext_shared,, -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1)
fi
