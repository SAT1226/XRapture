# - Find SLOP
# Find the SLOP libraries
#
#  This module defines the following variables:
#     SLOP_FOUND        - 1 if SLOP_INCLUDE_DIR & SLOP_LIBRARY are found, 0 otherwise
#     SLOP_INCLUDE_DIR  - where to find Xlib.h, etc.
#     SLOP_LIBRARY      - the X11 library
#
# Copyright (C) 2014 Dalton Nell, Maim Contributors (https://github.com/naelstrof/maim/graphs/contributors)
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

find_path( SLOP_INCLUDE_DIRS
           NAMES slop.hpp
           PATH_SUFFIXES /usr/include /include
           DOC "The SLOP include directory" )

find_library( SLOP_LIBRARIES
              NAMES slopy slopy.so slop slop.so
              PATHS /usr/lib /lib
              DOC "The SLOP library" )

FIND_PACKAGE(X11 REQUIRED)
FIND_PACKAGE(GLX REQUIRED)
list(APPEND SLOP_LIBRARIES
    ${X11_LIBRARIES}
    ${GLX_LIBRARY}
)

if( SLOP_INCLUDE_DIRS AND SLOP_LIBRARIES )
    set( SLOP_FOUND 1 )
else()
    set( SLOP_FOUND 0 )
endif()

mark_as_advanced( SLOP_INCLUDE_DIR SLOP_LIBRARY )
