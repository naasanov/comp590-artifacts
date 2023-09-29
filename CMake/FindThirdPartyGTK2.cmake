###############################################################################
# Software License Agreement (AGPL-3 License)
#
# Module to find GTK 2 library
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

if(EXISTS ${LIST_DEPENDENCIES_PATH}/gtk)
    set(ENV{PKG_CONFIG_PATH} "${LIST_DEPENDENCIES_PATH}/gtk/lib/pkgconfig")
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK2 REQUIRED gtk+-2.0)

if(GTK2_FOUND)
    ov_print(OV_PRINTED "Found GTK2 library")

    add_library(gtk2 INTERFACE)
    target_include_directories(gtk2 INTERFACE ${GTK2_INCLUDE_DIRS})
    target_link_directories(gtk2 INTERFACE ${GTK2_LIBRARY_DIRS})
    target_link_libraries(gtk2 INTERFACE ${GTK2_LIBRARIES})
    target_link_options(gtk2 INTERFACE ${GTK2_LDFLAGS})
endif()
