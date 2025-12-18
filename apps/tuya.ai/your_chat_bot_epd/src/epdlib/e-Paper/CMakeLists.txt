# Find all source files in a single current directory
# Save the name to DIR_ePaper_SRCS
aux_source_directory(. DIR_ePaper_SRCS)

include_directories(../Config)

# Generate the link library
add_library(ePaper ${DIR_ePaper_SRCS})
target_link_libraries(ePaper PUBLIC Config)

# Add include directories for T5AI platform
target_include_directories(ePaper PRIVATE 
    ${CMAKE_SOURCE_DIR}/platform/T5AI/tuyaos/tuyaos_adapter/include
    ${CMAKE_SOURCE_DIR}/platform/T5AI/tuyaos/tuyaos_adapter/include/utilities/include
    ${CMAKE_SOURCE_DIR}/platform/T5AI/tuyaos/tuyaos_adapter/include/spi/include
    ${CMAKE_SOURCE_DIR}/platform/T5AI/tuyaos/tuyaos_adapter/include/gpio
    ${CMAKE_SOURCE_DIR}/platform/T5AI/tuyaos/tuyaos_adapter/include/system
    ${CMAKE_SOURCE_DIR}/src/common/include
    ${CMAKE_SOURCE_DIR}/src/tuya_cloud_service/cloud
    ${CMAKE_SOURCE_DIR}/src/tal_system/include
    ${CMAKE_BINARY_DIR}/include
)