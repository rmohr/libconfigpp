#
# configure.ac
#
# Copyright (C) 2013 by
# Roman Mohr - <roman@fenkhuber.at>
# This file is part of libconfigpp.
#
# libconfigpp is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Lesser License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# libconfigpp is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Lesser License for more details.
#
# You should have received a copy of the GNU General Lesser License
# along with libconfigpp.  If not, see <http://www.gnu.org/licenses/>.

AC_PREREQ([2.59])
AC_INIT([libconfigpp], [0.1.0], [BUG-REPORT-ADDRESS])
AC_CONFIG_SRCDIR([include/libconfigpp.h])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([foreign])

# Checks for programs.
AC_PROG_CXX

# Checks for libraries.

# Checks for header files.

# Check for boost
AX_BOOST_BASE([1.20.0], [], [AC_MSG_ERROR(
               [Please install boost >= 1.20.0 (regex, filesystem, system])])
AX_BOOST_REGEX
AX_BOOST_FILESYSTEM
AX_BOOST_SYSTEM

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_TYPE_SIZE_T

# Checks for library functions.

AC_CONFIG_FILES([include/Makefile Makefile])

AC_OUTPUT
