cmake_minimum_required(VERSION 2.8.12)
project(abb_libegm)

find_package(Boost REQUIRED COMPONENTS system thread)

#############################
## Google Protocol Buffers ##
#############################
find_package(Protobuf REQUIRED)

# Make sure protoc is present, as apparently the above find_package() doesn't check that.
find_program(Protobuf_PROTOC_LOC NAMES protoc)
if (NOT Protobuf_PROTOC_LOC)
  message(FATAL_ERROR "Cannot find required 'protoc', cannot process Protobuf files without it. Aborting.")
endif()

# Generate C++ for protocol classes (headers and sources get written to the CMAKE_CURRENT_BINARY_DIR location).
set(EgmProtoFiles proto/egm.proto proto/egm_wrapper.proto proto/egm_wrapper_trajectory.proto)
if (NOT QUIET)
  message(STATUS "Generating protobuf C++ for: ${EgmProtoFiles}")
endif()
protobuf_generate_cpp(EgmProtoSources EgmProtoHeaders ${EgmProtoFiles})

if(NOT DEFINED BUILD_SHARED_LIBS)
  option(BUILD_SHARED_LIBS "Build dynamically-linked binaries" ON)
endif()

include(GNUInstallDirs)

###########
## Build ##
###########
set(SRC_FILES
    src/egm_base_interface.cpp
    src/egm_common.cpp
    src/egm_common_auxiliary.cpp
    src/egm_controller_interface.cpp
    src/egm_interpolator.cpp
    src/egm_logger.cpp
    src/egm_udp_server.cpp
    src/egm_trajectory_interface.cpp
    ${EgmProtoSources})

add_library(${PROJECT_NAME} ${SRC_FILES})
target_include_directories(${PROJECT_NAME} PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include;${CMAKE_CURRENT_BINARY_DIR}>")
target_include_directories(${PROJECT_NAME} PUBLIC ${Boost_INCLUDE_DIRS} ${PROTOBUF_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES} ${PROTOBUF_LIBRARIES})

#############
## Install ##
#############

install(TARGETS ${PROJECT_NAME}
        EXPORT ${PROJECT_NAME}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h")

install(FILES ${EgmProtoHeaders}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME})

install(FILES ${EgmProtoFiles}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME})

# Declare include directories for installed target
target_include_directories(${PROJECT_NAME} PUBLIC "$<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include>")

# Read version from the package.xml file
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/package.xml packageXmlContents)

string(REGEX MATCH "<version>([0-9]*.[0-9]*.[0-9]*)</version>" ver ${packageXmlContents})

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
"include(\"\${CMAKE_CURRENT_LIST_DIR}/${PROJECT_NAME}Targets.cmake\")
include(\"\${CMAKE_CURRENT_LIST_DIR}/${PROJECT_NAME}-extras.cmake\")
# Catkin compatibility
set(${PROJECT_NAME}_LIBRARIES ${PROJECT_NAME}::${PROJECT_NAME})
# The ${PROJECT_NAME}_INCLUDE_DIRS variable is empty because the include
# directories are already available in the ${PROJECT_NAME}::${PROJECT_NAME}
# imported target
set(${PROJECT_NAME}_INCLUDE_DIRS \" \")
"
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
  VERSION
    ${ver}
  COMPATIBILITY
    AnyNewerVersion
)

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/abb_libegm-extras.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/abb_libegm-extras.cmake
)

set(ConfigPackageLocation ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME})

install(
  FILES
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/abb_libegm-extras.cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/package.xml
  DESTINATION
    ${ConfigPackageLocation}
)

export(
  EXPORT
    ${PROJECT_NAME}
  FILE
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Targets.cmake"
  NAMESPACE
    ${PROJECT_NAME}::
)

install(
  EXPORT
    ${PROJECT_NAME}
  FILE
    ${PROJECT_NAME}Targets.cmake
  NAMESPACE
    ${PROJECT_NAME}::
  DESTINATION
    ${ConfigPackageLocation}
)


