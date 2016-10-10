cmake_minimum_required(VERSION 3.6)
set(CMAKE_CC_COMPILER gcc)
#set(CMAKE_BUILD_TYPE RELWITHDEBINFO)
#set(CMAKE_BUILD_TYPE DEBUG)
set(DCMAKE_CXX_COMPILER g++)
project(AcdFS)
include(ExternalProject)

    add_definitions(-DELPP_THREAD_SAFE -DELPP_STACKTRACE)
#add ccache
find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif(CCACHE_FOUND)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set (CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMake")
#include(cotire)

include_directories("/usr/local/include")
link_directories(/usr/lib)
link_directories(/usr/local/lib)

#add pkg-config
find_package(PkgConfig REQUIRED)

#add search for libbson
pkg_search_module(LIBBSON REQUIRED libbsoncxx)
include_directories(${LIBBSON_INCLUDE_DIRS})
link_directories(${LIBBSON_LIBRARY_DIRS})
link_libraries(bsoncxx)

#add search for libmongocxx
pkg_search_module(LIBMONGOCXX REQUIRED libmongocxx)
include_directories(${LIBMONGOCXX_INCLUDE_DIRS})
link_directories(${LIBMONGOCXX_LIB_DIRS})
link_libraries(mongocxx)

#add search for openssl
pkg_search_module(LIBOPENSSL REQUIRED libssl)
include_directories(/usr/local/opt/openssl/include)
link_directories(/usr/local/opt/openssl/lib)

#additonal libraries for ssl
#link_directories(/usr/lib)
link_libraries(ssl crypto)

#find_package (Threads)


#include BOOST
find_package(Boost REQUIRED thread system coroutine iostreams filesystem)
link_libraries(${Boost_LIBRARIES})
#message(${Boost_LIBRARIES})
#additional system include

# Check for FUSE.
#find_package (FUSE REQUIRED)
pkg_search_module(FUSE REQUIRED fuse)
LINK_LIBRARIES(${FUSE_LIBRARIES})
#include_directories (${FUSE_INCLUDE_DIR})
add_definitions (-D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=30)
#link_libraries(fuse3)

#add easylogging++ includes
#include_directories(${CMAKE_CURRENT_LIST_DIR}/deps/easyloggingpp/src)
#add_subdirectory(deps/easyloggingpp)
#include(deps/easyloggingpp)

find_package(CURL REQUIRED)
add_subdirectory(deps/cpr)
include_directories(${CPR_INCLUDE_DIRS})

#add encfs and fuse libraries
include_directories(deps/encfs/internal/)
#include_directories(deps/encfs)
include_directories(${LIBENCFS_CUSTOM_HEADERS_TEST})
link_directories(${CMAKE_BINARY_DIR})
#include_directories(${EASYLOGGINGPP_INCLUDE_DIR})
set(SOURCE_FILES main.cpp AcdApi.cpp AcdApi.h Account.cpp Account.h EncfsFilesystem.cpp EncfsFilesystem.h AcdObject.cpp AcdObject.h
        #        deps/encfs/ConfigReader.h deps/encfs/ConfigReader.cpp deps/encfs/FSConfig.h deps/encfs/Error.h deps/en  deps/encfs/ Error.cpp
        #        deps/encfs/ConfigVar.cpp deps/encfs/ConfigVar.h
        #        deps/encfs/base64.cpp deps/encfs/base64.h
        deps/encfs.patch
#        easylogging++.h
        EncFSEncryption.cpp EncFSEncryption.h Filesystem.cpp Filesystem.h FileIO.cpp FileIO.h)

#add_subdirectory(deps/encfs)
#include(deps/encfs)
#include(External_encfs)
#add_dependencies(AcdFS encfs)
#include(External_encfs)
#include(External_easyloggingpp)
#ExternalProject_Get_Property(encfs BINARY_DIR)
#set(LIBENCFS_CUSTOM_HEADERS_TEST ${BINARY_DIR})
#include_directories("${BINARY_DIR}")


#set(ENCFS_SOURCEFILES deps/src/encfs/encfs/ConfigReader.cpp)


set_source_files_properties(deps/encfs.patch PROPERTIES HEADER_FILE_ONLY TRUE)
include_directories(${ELPP_INCLUDE_INSTALL_DIR})

add_executable(AcdFS ${SOURCE_FILES}  ${ENCFS_SOURCEFILES} ${ELPP_INCLUDE_INSTALL_DIR} )
target_link_libraries(AcdFS ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(AcdFS ${CPR_LIBRARIES})

#target_include_directories(AcdFS deps/easyloggingpp/src)
#target_include_directories(AcdFS Easyloggingpp)

#add_dependencies(AcdFS encfs)
#add_dependencies(AcdFS deps/encfs.patch)
#cotire(AcdFS)


