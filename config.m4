
# vim: tabstop=4:softtabstop=4:shiftwidth=4:noexpandtab

# args
PHP_ARG_ENABLE(vyrtue,     whether to enable vyrtue support,
[AS_HELP_STRING([--enable-vyrtue], [Enable vyrtue support])])

PHP_ARG_ENABLE(vyrtue-debug, whether to enable vyrtue debug support,
[AS_HELP_STRING([--enable-vyrtue-debug], [Enable vyrtue debug support])], [no], [no])

AC_DEFUN([PHP_VYRTUE_ADD_SOURCES], [
  PHP_VYRTUE_SOURCES="$PHP_VYRTUE_SOURCES $1"
])

# main
if test "$PHP_VYRTUE" != "no"; then
    AC_MSG_CHECKING([if compiling with gcc])
    AC_COMPILE_IFELSE(
    [AC_LANG_PROGRAM([], [[
    #ifndef __GNUC__
        not gcc
    #endif
    ]])],
    [GCC=yes], [GCC=no])
    AC_MSG_RESULT([$GCC])

    AC_MSG_CHECKING([if compiling with clang])
    AC_COMPILE_IFELSE(
    [AC_LANG_PROGRAM([], [[
    #ifndef __clang__
        not clang
    #endif
    ]])],
    [CLANG=yes], [CLANG=no])
    AC_MSG_RESULT([$CLANG])

    AC_PATH_PROG(PKG_CONFIG, pkg-config, no)

    if test "$PHP_VYRTUE_DEBUG" == "yes"; then
        AC_DEFINE([VYRTUE_DEBUG], [1], [Enable vyrtue debug support])

        PHP_VYRTUE_ADD_SOURCES([
            src/debug.c
        ])
    fi

    PHP_VYRTUE_ADD_SOURCES([
        src/extension.c
        src/preprocess.c
        src/visitor.c
    ])

    PHP_ADD_BUILD_DIR(src)
    PHP_INSTALL_HEADERS([ext/vyrtue], [php_vyrtue.h])
    PHP_NEW_EXTENSION(vyrtue, $PHP_VYRTUE_SOURCES, $ext_shared, -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1)
    PHP_ADD_EXTENSION_DEP(vyrtue, ast, true)
    PHP_ADD_EXTENSION_DEP(vyrtue, opcache, true)
    PHP_SUBST(VYRTUE_SHARED_LIBADD)
fi
