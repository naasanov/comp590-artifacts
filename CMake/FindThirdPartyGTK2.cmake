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
    target_link_options(gtk2 INTERFACE ${GTK2_LDFLAGS})
    add_definitions(-DTARGET_HAS_ThirdPartyGTK)

    foreach(gtk_library ${GTK2_LIBRARIES})
        set(gtk_lib "GTK_LIB-NOTFOUND")
        find_library(gtk_lib NAMES ${gtk_library} PATHS ${GTK2_LIBRARY_DIRS} NO_DEFAULT_PATH)
        if (gtk_lib)
            target_link_libraries(gtk2 INTERFACE ${gtk_lib})
        endif()
    endforeach()
    

    if (WIN32)
        file(COPY ${GTK2_LIBRARY_DIRS}/gtk-2.0/i686-pc-vs10/engines/
             DESTINATION ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/gtk-2.0/i686-pc-vs10/engines/
             FILES_MATCHING PATTERN "*.dll")
        file(COPY ${GTK2_LIBRARY_DIRS}/../bin/
             DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
             FILES_MATCHING PATTERN "*.dll")

        if (EXISTS ${LIST_DEPENDENCIES_PATH}/gtk)
            install(DIRECTORY ${GTK2_LIBRARY_DIRS}/gtk-2.0/i686-pc-vs10/engines/
                    DESTINATION ${DIST_LIBDIR}/gtk-2.0/i686-pc-vs10/engines/
                    FILES_MATCHING PATTERN "*.dll")
            INSTALL(DIRECTORY ${GTK2_LIBRARY_DIRS}/../bin/
                DESTINATION ${DIST_BINDIR}
                FILES_MATCHING PATTERN "*.dll")
        endif()
    endif()
endif()
