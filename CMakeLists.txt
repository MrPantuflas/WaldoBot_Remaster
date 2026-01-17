## Headers
CMAKE_MINIMUM_REQUIRED(VERSION 3.5)

#Nombre proyecto

PROJECT(WaldoBot_Remaster 
        VERSION 0.1.1
        LANGUAGES CXX
	      DESCRIPTION "WaldoBot en C++")

set(CMAKE_CXX_STANDARD 20)
set(DPP_INSTALL OFF)
set(DPP_USE_EXTERNAL_JSON ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(ENABLE_DAVE OFF)
set(USE_SHARED_DPP ON CACHE BOOL "Use shared DPP library instead of building from source")

# Set build types
set(CMAKE_CONFIGURATION_TYPES "Release;Debug")

# Compiler flags for debug build
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -Wformat-security -pthread -fpermissive -pg -Og -D_FORTIFY_SOURCE=2 -D_GLIBCXX_ASSERTIONS -fstack-clash-protection -g -DDEBUG_MODE")

# Compiler flags for release build
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall -Ofast -fpermissive -pthread --param max-completely-peeled-insns=8 -fno-inline --param case-values-threshold=20")

# Set default build type if not specified
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build (Debug or Release)" FORCE)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20") # Add any additional flags common to both release and debug builds

file(GLOB_RECURSE WALDOBOT_HEADER_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)

file(GLOB_RECURSE WALDOBOT_SOURCE_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

#Dependencias

find_program(VALGRIND "valgrind")
if(VALGRIND)
    add_custom_target(valgrind COMMAND ${VALGRIND} --tool=callgrind ./bot ARGS)
endif()

set(BUILD_SHARED_LIBS OFF)

set(DPP_BUILD_TEST OFF CACHE BOOL "Disable DPP's own tests")
set(RUN_LDCONFIG "Run ldconfig after installation" OFF CACHE BOOL "Run ldconfig after installation")
set(DPP_NO_VCPKG "No VCPKG" ON CACHE BOOL "Disable VCPKG support")
set(DPP_NO_CONAN "No Conan" ON CACHE BOOL "Disable Conan support")
set(DPP_USE_EXTERNAL_JSON ON)

# Only modify DPP's json.h if we're building DPP from source
# Note: This is now handled in the DPP build section after FetchContent

include(ExternalProject)

set(OPUS_INSTALL_DIR ${CMAKE_BINARY_DIR}/opus-install)

ExternalProject_Add(opus_ext
    URL https://downloads.xiph.org/releases/opus/opus-1.5.2.tar.gz
    URL_HASH SHA256=65c1d2f78b9f2fb20082c38cbe47c951ad5839345876e46941612ee87f9a7ce1
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    PREFIX ${CMAKE_BINARY_DIR}/opus
    CONFIGURE_COMMAND ./configure --prefix=${OPUS_INSTALL_DIR} --enable-static --disable-shared
    BUILD_COMMAND make -j$(nproc)
    INSTALL_COMMAND make install
    BUILD_IN_SOURCE 1
)

add_library(opus_static STATIC IMPORTED GLOBAL)
set_target_properties(opus_static PROPERTIES
    IMPORTED_LOCATION ${OPUS_INSTALL_DIR}/lib/libopus.a
    INTERFACE_INCLUDE_DIRECTORIES ${OPUS_INSTALL_DIR}/include
)
add_dependencies(opus_static opus_ext)

set(OPUS_INCLUDE_DIR ${OPUS_INSTALL_DIR}/include CACHE PATH "Opus include dir")
set(OPUS_LIBRARIES ${OPUS_INSTALL_DIR}/lib/libopus.a CACHE FILEPATH "Opus library")

## load in pkg-config support
find_package(PkgConfig)
## use pkg-config to get hints for libDPP and liboggz locations
pkg_check_modules(PC_libDPP QUIET libDPP)
pkg_check_modules(PC_liboggz QUIET liboggz)

if(USE_SHARED_DPP)
    find_path(libDPP_INCLUDE_DIR
            NAMES dpp/dpp.h
            PATHS ${PC_libDPP_INCLUDE_DIRS}
            PATH_SUFFIXES include
            )

    find_library(libDPP_LIBRARY
            NAMES dpp
            PATHS ${PC_libDPP_LIBRARY_DIRS}
            )

    if(NOT libDPP_INCLUDE_DIR OR NOT libDPP_LIBRARY)
        message(WARNING "DPP library not found. Falling back to building from source.")
        message(STATUS "To install DPP: sudo apt install libdpp-dev (Ubuntu/Debian) or equivalent")
        set(USE_SHARED_DPP OFF CACHE BOOL "Use shared DPP library instead of building from source" FORCE)
    else()
        message(STATUS "Found shared DPP library: ${libDPP_LIBRARY}")
        
        # Test if the shared DPP library is compatible by checking required symbols
        message(STATUS "Testing DPP library compatibility...")
        
        # Create a simple test file
        file(WRITE ${CMAKE_BINARY_DIR}/test_dpp.cpp 
            "#include <dpp/dpp.h>\nint main() { dpp::cluster bot(\"\"); return 0; }")
        
        try_compile(DPP_COMPATIBILITY_TEST
            ${CMAKE_BINARY_DIR}/dpp_test
            ${CMAKE_BINARY_DIR}/test_dpp.cpp
            LINK_LIBRARIES ${libDPP_LIBRARY}
            CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${libDPP_INCLUDE_DIR}"
            OUTPUT_VARIABLE DPP_TEST_OUTPUT
        )
        
        if(NOT DPP_COMPATIBILITY_TEST)
            message(WARNING "Shared DPP library appears to be incompatible (ABI mismatch).")
            message(STATUS "Test output: ${DPP_TEST_OUTPUT}")
            message(STATUS "This usually means the DPP library was built with a newer compiler/glibc.")
            message(STATUS "Falling back to building DPP from source...")
            set(USE_SHARED_DPP OFF CACHE BOOL "Use shared DPP library instead of building from source" FORCE)
        else()
            message(STATUS "DPP library compatibility test passed.")
            message(STATUS "Using shared DPP library: ${libDPP_LIBRARY}")
            
            # Create nlohmann directory structure for yt-search.h compatibility
            file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/nlohmann")
            if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/include/nlohmann/json.hpp")
                file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/include/json.hpp" 
                     DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/include/nlohmann/")
                message(STATUS "Created nlohmann/json.hpp symlink for yt-search.h compatibility")
            endif()
        endif()
    endif()
endif()

find_package(OpenSSL REQUIRED)
find_package(CURL)

# Find zlib - provide hints for systems where it's not automatically detected
find_package(ZLIB)
if(NOT ZLIB_FOUND)
    # Try common paths as hints
    find_path(ZLIB_INCLUDE_DIR zlib.h
        HINTS /usr/include /usr/local/include
    )
    find_library(ZLIB_LIBRARY
        NAMES z zlib
        HINTS /usr/lib /usr/local/lib /usr/lib64 /usr/lib/x86_64-linux-gnu
    )
    if(ZLIB_INCLUDE_DIR AND ZLIB_LIBRARY)
        set(ZLIB_FOUND TRUE)
        message(STATUS "Found ZLIB: ${ZLIB_LIBRARY}")
    endif()
endif()

if(CURL_FOUND)
    message(STATUS "Found CURL: ${CURL_LIBRARIES}")
else()
    message(WARNING "CURL not found. curlpp will not be available. Install libcurl4-openssl-dev or libcurl-dev")
endif()

find_path(liboggz_INCLUDE_DIR
        NAMES oggz.h
        PATHS ${PC_liboggz_INCLUDE_DIRS}
        )

find_library(liboggz_LIBRARY
        NAMES oggz
        PATHS ${PC_liboggz_LIBRARY_DIRS}
        )

include(FetchContent)

# Download DotenvCpp if not found
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libs/DotenvCpp/CMakeLists.txt")
        message(STATUS "Using local DotenvCpp in libs/DotenvCpp")
        add_subdirectory(libs/DotenvCpp)
else()
        message(STATUS "DotenvCpp not found, downloading...")
        FetchContent_Declare(
                DotenvCpp
                GIT_REPOSITORY https://github.com/atrox39/DotenvCpp.git
                GIT_TAG        master
        )
        FetchContent_MakeAvailable(DotenvCpp)
endif()

# Download opus if not found
#if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libs/opus")
#        message(STATUS "Using local opus in libs/opus")
#        add_subdirectory(libs/opus)
#else()
#        message(STATUS "opus not found, downloading...")
#        FetchContent_Declare(
#                opus
#                GIT_REPOSITORY https://github.com/xiph/opus.git
#                GIT_TAG        master
#        )
#        FetchContent_Populate(opus)
#        file(COPY "${opus_SOURCE_DIR}" DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/libs/")
#endif()

# Download and build DPP only if not using shared library
if(NOT USE_SHARED_DPP)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libs/DPP/CMakeLists.txt")
        message(STATUS "Using local DPP in libs/DPP")
        # Modify json.h for local DPP
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libs/DPP/include/dpp/json.h")
            execute_process(COMMAND sed -i "s/#include <nlohmann\\/json.hpp>/#include \"nlohmann\\/json.hpp\"/" "${CMAKE_CURRENT_SOURCE_DIR}/libs/DPP/include/dpp/json.h")
        endif()
        add_subdirectory(libs/DPP)
    else()
        message(STATUS "DPP not found locally, fetching with FetchContent...")
        FetchContent_Declare(
            DPP
            GIT_REPOSITORY https://github.com/brainboxdotcc/DPP.git
            GIT_TAG        master
        )
        FetchContent_Populate(DPP)
        
        # Modify json.h for FetchContent DPP
        if(EXISTS "${dpp_SOURCE_DIR}/include/dpp/json.h")
            execute_process(COMMAND sed -i "s/#include <nlohmann\\/json.hpp>/#include \"nlohmann\\/json.hpp\"/" "${dpp_SOURCE_DIR}/include/dpp/json.h")
            message(STATUS "Modified DPP json.h to use local nlohmann/json.hpp")
        endif()
        
        add_subdirectory(${dpp_SOURCE_DIR} ${dpp_BINARY_DIR})
    endif()
else()
    message(STATUS "Skipping DPP build - using shared library")
endif()

# Download yt-search.h if not found (source files only, no library build)
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libs/yt-search.h/include")
        message(STATUS "yt-search.h not found, downloading...")
        FetchContent_Declare(
                yt-search.h
                GIT_REPOSITORY https://github.com/Neko-Life/yt-search.h.git
                GIT_TAG        main
        )
        FetchContent_Populate(yt-search.h)
        if(yt-search.h_SOURCE_DIR)
            file(COPY "${yt-search.h_SOURCE_DIR}/" DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/libs/yt-search.h/")
        endif()
else()
        message(STATUS "Using local yt-search.h source files")
endif()

# Note: We don't call add_subdirectory for yt-search.h because we compile its sources directly in our bot target

# Download curlpp if not found (requires CURL)
if(CURL_FOUND)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libs/curlpp/CMakeLists.txt")
            message(STATUS "Using local curlpp in libs/curlpp")
            add_subdirectory(libs/curlpp)
    else()
            message(STATUS "curlpp not found, downloading...")
            FetchContent_Declare(
                    curlpp
                    GIT_REPOSITORY https://github.com/jpbarrette/curlpp.git
                    GIT_TAG        master
            )
            FetchContent_Populate(curlpp)
            if(curlpp_SOURCE_DIR)
                file(COPY "${curlpp_SOURCE_DIR}/" DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/libs/curlpp/")
            endif()
            add_subdirectory(libs/curlpp)
    endif()
else()
    message(STATUS "Skipping curlpp - CURL not found")
endif()

# Download liboggz if not found
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libs/liboggz")
        message(STATUS "liboggz not found, downloading...")
        FetchContent_Declare(
                liboggz
                GIT_REPOSITORY https://gitlab.xiph.org/xiph/liboggz.git
                GIT_TAG        master
        )
        FetchContent_Populate(liboggz)
        if(liboggz_SOURCE_DIR)
            file(COPY "${liboggz_SOURCE_DIR}/" DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/libs/liboggz/")
        endif()
endif()

# Download OpenSSL if not found

#Ejecutable

ADD_EXECUTABLE(bot 
        ${WALDOBOT_HEADER_FILES}
        include/json.hpp
        libs/yt-search.h/src/yt-search/encode.cpp
	libs/yt-search.h/src/yt-search/yt-playlist.cpp
	libs/yt-search.h/src/yt-search/yt-search.cpp
	libs/yt-search.h/src/yt-search/yt-track-info.cpp
        ${WALDOBOT_SOURCE_FILES}
)

TARGET_LINK_LIBRARIES(bot
        dpp
        oggz
        dotenv
        ${WaldoBot_LIBRARIES}
        ${liboggz_LIBRARY}
)

# Only link system DPP library when using shared DPP
if(USE_SHARED_DPP)
    TARGET_LINK_LIBRARIES(bot ${libDPP_LIBRARY})
endif()

# Add MLS libraries only when building DPP from source
if(NOT USE_SHARED_DPP)
    TARGET_LINK_LIBRARIES(bot
        mlspp
        mls_vectors
        bytes
        tls_syntax
        hpke
    )
endif()

# Add curlpp and CURL only if available
if(CURL_FOUND)
    TARGET_LINK_LIBRARIES(bot curlpp ${CURL_LIBRARIES})
endif()

TARGET_INCLUDE_DIRECTORIES(bot PRIVATE
        include
        libs
        libs/yt-search.h/include
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/yt-search.h/include
)

# Add DotenvCpp include path
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libs/DotenvCpp/src")
    TARGET_INCLUDE_DIRECTORIES(bot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/libs/DotenvCpp/src)
else()
    # Use FetchContent source directory
    TARGET_INCLUDE_DIRECTORIES(bot PRIVATE ${dotenvcpp_SOURCE_DIR}/src)
endif()

# Add curlpp include only if CURL is available
if(CURL_FOUND)
    TARGET_INCLUDE_DIRECTORIES(bot PRIVATE libs/curlpp/include)
endif()

if(NOT USE_SHARED_DPP)
    TARGET_INCLUDE_DIRECTORIES(bot PRIVATE
        libs/DPP/include
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/DPP/include
    )
    # When building DPP from source via FetchContent, also add those include paths
    if(dpp_SOURCE_DIR)
        TARGET_INCLUDE_DIRECTORIES(bot PRIVATE ${dpp_SOURCE_DIR}/include)
    endif()
else()
    TARGET_INCLUDE_DIRECTORIES(bot PRIVATE ${libDPP_INCLUDE_DIR})
endif()

if (OPENSSL_FOUND)
  # Add the include directories for compiling
  target_include_directories(bot PUBLIC ${OPENSSL_INCLUDE_DIR})

  # Add the static lib for linking
  target_link_libraries(bot OpenSSL::SSL OpenSSL::Crypto)

  message(STATUS "Found OpenSSL ${OPENSSL_VERSION}")

else()

  message(STATUS "OpenSSL Not Found")

endif()

# Add MLS library directories only when building DPP from source
if(NOT USE_SHARED_DPP)
    TARGET_LINK_DIRECTORIES(bot PRIVATE
        ${CMAKE_BINARY_DIR}/libs/DPP/library/mlspp
        ${CMAKE_BINARY_DIR}/libs/DPP/library/mlspp/lib/bytes
        ${CMAKE_BINARY_DIR}/libs/DPP/library/mlspp/lib/hpke
        ${CMAKE_BINARY_DIR}/libs/DPP/library/mlspp/lib/mls_vectors
        ${CMAKE_BINARY_DIR}/libs/DPP/library/mlspp/lib/tls_syntax
    )
    # But when using FetchContent, the paths are different
    if(dpp_SOURCE_DIR)
        TARGET_LINK_DIRECTORIES(bot PRIVATE
            ${dpp_BINARY_DIR}/library/mlspp
            ${dpp_BINARY_DIR}/library/mlspp/lib/bytes
            ${dpp_BINARY_DIR}/library/mlspp/lib/hpke
            ${dpp_BINARY_DIR}/library/mlspp/lib/mls_vectors
            ${dpp_BINARY_DIR}/library/mlspp/lib/tls_syntax
        )
    endif()
endif()

# Add curlpp build directory only if CURL is available
if(CURL_FOUND)
    TARGET_LINK_DIRECTORIES(bot PRIVATE libs/curlpp/build)
endif()

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/downloads")