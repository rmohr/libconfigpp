#
# Makefile.am
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

TESTS = simple_test test_runner
noinst_PROGRAMS = simple_test test_runner
simple_test_LDFLAGS = -lboost_system
simple_test_CPPFLAGS = -I$(top_srcdir)/include
simple_test_SOURCES = simple_test.cpp

test_runner_LDFLAGS = -lboost_system -lboost_unit_test_framework -lboost_filesystem -lboost_regex
test_runner_CPPFLAGS = -I$(top_srcdir)/include
test_runner_SOURCES = test_runner.cpp

EXTRA_DIST = simple_config.cfg
