##
# @file CMakeLists.txt
# @brief buddy_pixel - Claude Desktop Buddy for TUYA_T5AI_PIXEL
#/

set(APP_PATH ${CMAKE_CURRENT_LIST_DIR})
get_filename_component(APP_NAME ${APP_PATH} NAME)

aux_source_directory(${APP_PATH}/src APP_SRC)
# GIF C arrays live in a subdirectory — collect them separately
aux_source_directory(${APP_PATH}/src/claude_status_gif GIF_SRC)

set(APP_INC
    ${APP_PATH}/src
    ${APP_PATH}/src/claude_status_gif
)

########################################
# Target Configure
########################################
add_library(${EXAMPLE_LIB})

target_sources(${EXAMPLE_LIB}
    PRIVATE
        ${APP_SRC}
        ${GIF_SRC}
    )

target_include_directories(${EXAMPLE_LIB}
    PRIVATE
        ${APP_INC}
    )
