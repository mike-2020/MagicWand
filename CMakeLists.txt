# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS ../SmartGarden-Lib/components)

set(include_dirs 
#    ${CMAKE_SOURCE_DIR}/components/smart_sound_board
    )

include($ENV{ADF_PATH}/CMakeLists.txt)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
get_filename_component(ProjectId ${CMAKE_CURRENT_LIST_DIR} NAME)
string(REPLACE " " "_" ProjectId ${ProjectId})
project(${ProjectId})

fatfs_create_rawflash_image("res" "./res/" FLASH_IN_PROJECT)


