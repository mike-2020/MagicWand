#set(voice_data_image $ENV{ADF_PATH}/components/esp-sr/esp-tts/esp_tts_chinese/esp_tts_voice_data_xiaole.dat)
#add_custom_target(voice_data ALL DEPENDS ${voice_data_image})
#add_dependencies(flash voice_data)

#partition_table_get_partition_info(size "--partition-name voice_data" "size")
#partition_table_get_partition_info(offset "--partition-name voice_data" "offset")

#if("${size}" AND "${offset}")
#    esptool_py_flash_to_partition(flash "voice_data" "${voice_data_image}")
#else()
#    set(message "Failed to find model in partition table file"
#                "Please add a line(Name=voice_data, Type=data, Size=3890K) to the partition file.")
#endif()

set(MODEL_FOLDER ../TinyML)
set(EI_SDK_FOLDER ../TinyML/edge-impulse-sdk) 

if(NOT CMAKE_BUILD_EARLY_EXPANSION)
# add_definitions(-DEI_CLASSIFIER_TFLITE_ENABLE_ESP_NN=1) # enables ESP-NN optimizations by Espressif
# disabled by default - enable if you use ESP32S3
 add_definitions(-DEI_CLASSIFIER_TFLITE_ENABLE_ESP_NN_S3=1) # enables ESP-NN S3 optimizations by Espressif
endif()

OPTION(DEFINE_DEBUG
    "Build the project using debugging code"
    OFF)
IF(DEFINE_DEBUG)
    MESSAGE("Adding Debug flag...")
    ADD_DEFINITIONS(-DDEBUG)
    SET(CMAKE_BUILD_TYPE Debug)
    MESSAGE("Build type is " ${CMAKE_BUILD_TYPE})
ENDIF(DEFINE_DEBUG)


include(${EI_SDK_FOLDER}/cmake/utils.cmake)

RECURSIVE_FIND_FILE_EXCLUDE_DIR(SOURCE_FILES "${EI_SDK_FOLDER}" "CMSIS" "*.cpp")
RECURSIVE_FIND_FILE_EXCLUDE_DIR(MODEL_FILES "${MODEL_FOLDER}/tflite-model" "CMSIS" "*.cpp")
RECURSIVE_FIND_FILE_EXCLUDE_DIR(CC_FILES "${EI_SDK_FOLDER}" "CMSIS" "*.cc")
RECURSIVE_FIND_FILE_EXCLUDE_DIR(S_FILES "${EI_SDK_FOLDER}" "CMSIS" "*.s")
RECURSIVE_FIND_FILE_EXCLUDE_DIR(C_FILES "${EI_SDK_FOLDER}" "CMSIS" "*.c")

list(APPEND SOURCE_FILES ${S_FILES})
list(APPEND SOURCE_FILES ${C_FILES})
list(APPEND SOURCE_FILES ${CC_FILES})
list(APPEND SOURCE_FILES ${MODEL_FILES})


set(include_dirs 
    include
    RemoteController
    LLM
    Display
    ${IDF_PATH}/components/json/
    ${IDF_PATH}/components/ulp/ulp_riscv/include
    ${IDF_PATH}/components/ulp/ulp_riscv/ulp_core/include
    ${IDF_PATH}/components/riscv/include
    ${MODEL_FOLDER}
    ${MODEL_FOLDER}/tflite-model
    ${MODEL_FOLDER}/model-parameters
    ${MODEL_FOLDER}/ingestion-sdk-c
    ${EI_SDK_FOLDER}
    )

file(GLOB MAIN_FILES *.c *.cpp)
list(APPEND native_srcs ${MAIN_FILES})
file(GLOB RC_FILES RemoteController/*.c RemoteController/*.cpp LLM/*.cpp Display/*.cpp Display/*.c)
list(APPEND native_srcs ${RC_FILES})
list(APPEND native_srcs ${SOURCE_FILES})
#set(requires fatfs console wear_levelling esp-sr  audio_hal audio_stream audio_pipeline esp_peripherals smart_utilities audio_recorder TDOA SpeakerVerification tinyusb esp_tinyusb)
#set(requires esp_http_server audio_hal audio_stream audio_pipeline audio_recorder esp_peripherals TDOA)

idf_component_register(SRCS "${native_srcs}"
                       INCLUDE_DIRS "${include_dirs}"
                       PRIV_REQUIRES ""
                       EMBED_FILES "../resources/favicon.ico" "../resources/upload_script.html"
                       #REQUIRES riscv
                       )

add_definitions(-w)
#target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++17)


# ULP support additions to component CMakeLists.txt.
#
# 1. The ULP app name must be unique (if multiple components use ULP).
set(ulp_app_name ulp_${COMPONENT_NAME})
#
# 2. Specify all C and Assembly source files.
#    Files should be placed into a separate directory (in this case, ulp/),
#    which should not be added to COMPONENT_SRCS.
set(ulp_riscv_sources "ulp/main.c")

#
# 3. List all the component source files which include automatically
#    generated ULP export file, ${ulp_app_name}.h:
set(ulp_exp_dep_srcs "ulp_main.c")

#
# 4. Call function to build ULP binary and embed in project using the argument
#    values above.
ulp_embed_binary(${ulp_app_name} "${ulp_riscv_sources}" "${ulp_exp_dep_srcs}")

