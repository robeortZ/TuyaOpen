# If EXAMPLE_NAME is defined in the upper layer, add this module to the existing example project
if(DEFINED EXAMPLE_NAME)
    set(APP_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})

    set(APP_MODULE_SRCS)
    file(GLOB_RECURSE APP_MODULE_SRCS "${APP_MODULE_PATH}/*.c")
    
    # Exclude zlib test and example files that have external dependencies
    list(FILTER APP_MODULE_SRCS EXCLUDE REGEX ".*/zlib/contrib/testzlib/.*")
    list(FILTER APP_MODULE_SRCS EXCLUDE REGEX ".*/zlib/contrib/untgz/.*")
    list(FILTER APP_MODULE_SRCS EXCLUDE REGEX ".*/zlib/contrib/minizip/.*")  # Exclude all contrib/minizip (we use libminizip)
    list(FILTER APP_MODULE_SRCS EXCLUDE REGEX ".*/zlib/contrib/infback9/.*")  # Exclude infback9
    list(FILTER APP_MODULE_SRCS EXCLUDE REGEX ".*/zlib/examples/.*")
    list(FILTER APP_MODULE_SRCS EXCLUDE REGEX ".*/zlib/example\\.c$")
    list(FILTER APP_MODULE_SRCS EXCLUDE REGEX ".*/zlib/test/.*")
    
    # Exclude zlib gzip file operations (use POSIX read/write/close not available in embedded)
    list(FILTER APP_MODULE_SRCS EXCLUDE REGEX ".*/zlib/gzread\\.c$")
    list(FILTER APP_MODULE_SRCS EXCLUDE REGEX ".*/zlib/gzwrite\\.c$")
    list(FILTER APP_MODULE_SRCS EXCLUDE REGEX ".*/zlib/gzlib\\.c$")
    list(FILTER APP_MODULE_SRCS EXCLUDE REGEX ".*/zlib/gzclose\\.c$")
    
    # Exclude the original ioapi.c in libminizip, we use ioapi_tkl.c instead
    list(FILTER APP_MODULE_SRCS EXCLUDE REGEX ".*/libminizip/ioapi\\.c$")
    
    # Exclude minizip command-line tools (they depend on RT-Thread rtconfig.h)
    list(FILTER APP_MODULE_SRCS EXCLUDE REGEX ".*/libminizip/miniunz\\.c$")
    list(FILTER APP_MODULE_SRCS EXCLUDE REGEX ".*/libminizip/minizip\\.c$")
    
    # Exclude example and test files
    list(FILTER APP_MODULE_SRCS EXCLUDE REGEX ".*/epub_reader/example_usage\\.c$")

    set(APP_MODULE_INC
        ${APP_MODULE_PATH}
        ${APP_MODULE_PATH}/ebook
        ${APP_MODULE_PATH}/epub_reader/include
        ${APP_MODULE_PATH}/epub_reader/src/zlib
        ${APP_MODULE_PATH}/epub_reader/src/libminizip
    )

    # Add the source files and header files in the current directory to the existing ${EXAMPLE_LIB}
    target_sources(${EXAMPLE_LIB}
        PRIVATE
            ${APP_MODULE_SRCS}
    )
    target_include_directories(${EXAMPLE_LIB}
        PRIVATE
            ${APP_MODULE_INC}
    )
    
    # Disable -Werror for app_ui module to allow warnings
    target_compile_options(${EXAMPLE_LIB}
        PRIVATE
            -Wno-error
    )

else()
    # Default standalone mode: build a static ui library from this directory

    # Search for all .c source files in CMAKE_CURRENT_SOURCE_DIR (recursively)
    file(GLOB_RECURSE SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    )

    add_library(ui ${SOURCES})

    target_include_directories(ui PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}           # Current directory, for ui.h and other local headers
        ${CMAKE_SOURCE_DIR}                   # Project root, for lvgl/lvgl.h
        ${CMAKE_SOURCE_DIR}/src/cJSON         # cJSON header directory
    )

    target_link_libraries(ui PUBLIC lvgl)
endif()
