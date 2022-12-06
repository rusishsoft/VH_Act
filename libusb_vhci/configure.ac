AC_PREREQ([2.63])
AC_INIT([libusb_vhci], [0.8])
AC_CONFIG_SRCDIR([src/libusb_vhci.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([foreign dist-bzip2 tar-ustar])
AM_MAINTAINER_MODE

LT_PREREQ([2.2.6])
LT_INIT

MY_CHECK_WS

MY_ARG_ENABLE(
    debug,
    no,
    [Compile with debugging flags.]
)

AM_CONDITIONAL(ENABLE_DEBUG, [test "x$enable_debug" != "xno"])
AM_CONDITIONAL(ENABLE_RELEASE, [test "x$enable_debug" = "xno"])

if test "x$enable_debug" != "xno"; then
	CFLAGS="-g3 -O0 $CFLAGS"
	CXXFLAGS="-g3 -O0 $CXXFLAGS"
else
	CFLAGS="-O2 $CFLAGS"
	CXXFLAGS="-O2 $CXXFLAGS"
fi

# Checks for programs.
AC_LANG_C
AC_PROG_CC
AC_PROG_CC_STDC
AM_PROG_CC_C_O
AC_PROG_CXX
AC_PROG_LIBTOOL
AC_PROG_INSTALL

# Checks for libraries.
AC_CHECK_LIB([pthread], [pthread_self], [],
	[AC_MSG_ERROR([pthread library is missing])]
)

# Checks for header files.
AC_CHECK_HEADERS([pthread.h stdlib.h stdint.h stdio.h string.h errno.h fcntl.h unistd.h sys/ioctl.h], [],
	[AC_MSG_ERROR([missing header files])]
)
AC_CHECK_HEADERS([linux/ioctl.h], [],
	[AC_MSG_ERROR([missing linux kernel header files])]
)
AC_CHECK_HEADERS([linux/usb-vhci.h], [],
	[AC_MSG_ERROR([missing usb-vhci header files; install header files from usb_vhci_hcd package])]
)

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([memset])

AC_CONFIG_FILES([
Makefile
src/Makefile
examples/Makefile
])

AC_OUTPUT
