
m4_include(m4/ax_require_defined.m4)
m4_include(m4/ax_prepend_flag.m4)
m4_include(m4/ax_compiler_vendor.m4)
m4_include(m4/ax_cflags_warn_all.m4)

PHP_ARG_ENABLE(vyrtue,     whether to enable vyrtue support,
[AS_HELP_STRING([--enable-vyrtue], [Enable vyrtue support])])

PHP_ARG_ENABLE(vyrtue-debug, whether to enable vyrtue debug support,
[AS_HELP_STRING([--enable-vyrtue-debug], [Enable vyrtue debug support])], [no], [no])

AC_DEFUN([PHP_VYRTUE_ADD_SOURCES], [
  PHP_VYRTUE_SOURCES="$PHP_VYRTUE_SOURCES $1"
])

if test "$PHP_VYRTUE" != "no"; then
    AX_CFLAGS_WARN_ALL([WARN_CFLAGS])
    CFLAGS="$WARN_CFLAGS $CFLAGS"

    if test "$PHP_VYRTUE_DEBUG" == "yes"; then
        AC_DEFINE([VYRTUE_DEBUG], [1], [Enable vyrtue debug support])

        PHP_VYRTUE_ADD_SOURCES([
            src/debug.c
        ])
    fi

    PHP_VYRTUE_ADD_SOURCES([
        src/compile.c
        src/context.c
        src/extension.c
        src/process.c
        src/visitor.c
    ])

    PHP_ADD_BUILD_DIR(src)
    PHP_INSTALL_HEADERS([ext/vyrtue], [php_vyrtue.h])
    PHP_NEW_EXTENSION(vyrtue, $PHP_VYRTUE_SOURCES, $ext_shared, -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1)
    PHP_ADD_EXTENSION_DEP(vyrtue, ast, true)
    PHP_ADD_EXTENSION_DEP(vyrtue, opcache, true)
    PHP_SUBST(VYRTUE_SHARED_LIBADD)
fi
