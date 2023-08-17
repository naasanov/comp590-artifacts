###############################################################################
# Software License Agreement (AGPL-3 License)
#
# Module to find Vorbis library
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

# Find OpenAL qnd FreeALUT when packages are available in conda
find_package(OpenAL REQUIRED)
find_package(Ogg REQUIRED)
find_package(Vorbis REQUIRED)

if (OpenAL_FOUND AND OGG_FOUND AND Vorbis_FOUND)
    ov_print(OV_PRINTED "Found OpenAL Vorbis and OGG")
    target_compile_definitions(OpenAL::OpenAL INTERFACE -DTARGET_HAS_ThirdPartyOpenAL)
else()
    ov_print(OV_PRINTED "Failed to find OpenAL, Vorbis or OGG")
endif()