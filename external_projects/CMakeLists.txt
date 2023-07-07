###############################################################################
# Software License Agreement (AGPL-3 License)
#
# Module to include in order to get all needed dependencies for OpenViBE
# as CMake targets directly.
#
# Copyright (C) Inria 2021
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License version 3,
# as published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program.
# If not, see <http://www.gnu.org/licenses/>.
###############################################################################
cmake_minimum_required(VERSION 3.16)

cmake_policy(SET CMP0097 NEW)

project(Dependencies)

# #############################################################################
# Setup
# #############################################################################

set(EP_DEPENDENCIES_DIR "${CMAKE_SOURCE_DIR}/../dependencies/" CACHE PATH "Directory where dependencies should be installed")
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/CMake)
set(EP_DEPENDENCIES_WORK_DIR ${CMAKE_SOURCE_DIR}/arch/${CMAKE_GENERATOR_PLATFORM}/${CMAKE_BUILD_TYPE} CACHE PATH "Directory where dependencies will be downloaded and built")

if ("${EP_DEPENDENCIES_DIR}" STREQUAL "")
    message(FATAL_ERROR "EP_DEPENDENCIES_DIR variable required")
endif()

#include(Boost)
#include(Eigen)
#include(GTest)
include(Lua)
#include(TinyXML2)
include(VRPN)
