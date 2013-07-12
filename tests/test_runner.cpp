/*
 test_runner.cpp

 Copyright (C) 2013 by
 Roman Mohr - <roman@fenkhuber.at>
 This file is part of libconfigpp.

 libconfigpp is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Lesser License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 libconfigpp is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Lesser License for more details.

 You should have received a copy of the GNU General Lesser License
 along with libconfigpp.  If not, see <http://www.gnu.org/licenses/>.
*/

#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Hello
#include <boost/test/unit_test.hpp>
#include <libconfigpp.h>

BOOST_AUTO_TEST_CASE(read_simple_config)
{
    libconfig::Config cfg("simple_config.cfg");
    int int_value = cfg["int"];
    double double_value = cfg["double"];
    std::string string_value = cfg["string"];

    BOOST_CHECK_EQUAL(int_value,1);
    BOOST_CHECK_CLOSE(double_value,2.34,0.001);
    BOOST_CHECK_EQUAL(string_value,"string");
}

