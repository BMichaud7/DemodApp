cmake_minimum_required(VERSION 3.20)
project(DemodApp VERSION 1.0.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

# ── SdrTaskApi ────────────────────────────────────────────────────────────────
get_filename_component(PARENT "${CMAKE_SOURCE_DIR}" DIRECTORY)
if(EXISTS "${PARENT}/SdrSdk/CMakeLists.txt")
    set(SDRSDK_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SDRSDK_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
    add_subdirectory("${PARENT}/SdrSdk" "${CMAKE_BINARY_DIR}/SdrSdk")
elseif(EXISTS "${PARENT}/SdrTaskApi/CMakeLists.txt")
    add_subdirectory("${PARENT}/SdrTaskApi" "${CMAKE_BINARY_DIR}/SdrTaskApi")
else()
    message(FATAL_ERROR "SdrTaskApi not found as a sibling directory")
endif()

# ── liquid-dsp ────────────────────────────────────────────────────────────────
pkg_check_modules(LIQUID QUIET IMPORTED_TARGET liquid)
if(LIQUID_FOUND)
    message(STATUS "[DemodApp] liquid-dsp: found via pkg-config ${LIQUID_VERSION}")
else()
    find_library(LIQUID_LIB liquid)
    find_path(LIQUID_INCLUDE_DIR liquid/liquid.h)
    if(LIQUID_LIB AND LIQUID_INCLUDE_DIR)
        add_library(PkgConfig::LIQUID INTERFACE IMPORTED)
        target_include_directories(PkgConfig::LIQUID INTERFACE ${LIQUID_INCLUDE_DIR})
        target_link_libraries(PkgConfig::LIQUID INTERFACE ${LIQUID_LIB})
        set(LIQUID_FOUND TRUE)
        message(STATUS "[DemodApp] liquid-dsp: found without pkg-config")
    else()
        message(FATAL_ERROR "[DemodApp] liquid-dsp not found.\n"
            "  Ubuntu/Debian: apt-get install libliquid-dev\n"
            "  RHEL/Fedora:   dnf install liquid-dsp-devel\n"
            "  Or build inside the container where it is always available.")
    endif()
endif()

# ── qpid-proton C++ ───────────────────────────────────────────────────────────
find_package(ProtonCpp QUIET)
if(NOT ProtonCpp_FOUND)
    # Ubuntu 24.04 pkg-config name may be libqpid-proton-cpp or qpid-proton-cpp
    pkg_check_modules(PROTON QUIET IMPORTED_TARGET libqpid-proton-cpp)
    if(NOT PROTON_FOUND)
        pkg_check_modules(PROTON QUIET IMPORTED_TARGET qpid-proton-cpp)
    endif()
    if(NOT PROTON_FOUND)
        # Last resort: find_library + find_path
        find_library(PROTON_CPP_LIB qpid-proton-cpp)
        find_path(PROTON_CPP_INCLUDE_DIR proton/messaging_handler.hpp
            PATH_SUFFIXES include)
        if(PROTON_CPP_LIB AND PROTON_CPP_INCLUDE_DIR)
            add_library(PkgConfig::PROTON INTERFACE IMPORTED)
            target_include_directories(PkgConfig::PROTON INTERFACE ${PROTON_CPP_INCLUDE_DIR})
            target_link_libraries(PkgConfig::PROTON INTERFACE ${PROTON_CPP_LIB})
            set(PROTON_FOUND TRUE)
            message(STATUS "[DemodApp] qpid-proton-cpp: found without pkg-config")
        else()
            message(FATAL_ERROR "[DemodApp] qpid-proton-cpp not found.\n"
                "  Ubuntu: apt-get install libqpid-proton-cpp12-dev")
        endif()
    endif()
endif()

# ── nlohmann/json ─────────────────────────────────────────────────────────────
find_package(nlohmann_json 3.11 QUIET)
if(NOT nlohmann_json_FOUND)
    include(FetchContent)
    FetchContent_Declare(nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.11.3)
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# ── spdlog ────────────────────────────────────────────────────────────────────
find_package(spdlog QUIET)
if(NOT spdlog_FOUND)
    pkg_check_modules(SPDLOG QUIET IMPORTED_TARGET spdlog)
    if(SPDLOG_FOUND)
        add_library(spdlog::spdlog INTERFACE IMPORTED)
        target_include_directories(spdlog::spdlog INTERFACE ${SPDLOG_INCLUDE_DIRS})
        target_link_libraries(spdlog::spdlog INTERFACE ${SPDLOG_LIBRARIES})
        target_compile_definitions(spdlog::spdlog INTERFACE SPDLOG_FMT_EXTERNAL)
    else()
        include(FetchContent)
        FetchContent_Declare(spdlog
            GIT_REPOSITORY https://github.com/gabime/spdlog.git
            GIT_TAG        v1.13.0)
        FetchContent_MakeAvailable(spdlog)
    endif()
endif()

# ── tinyxml2 ──────────────────────────────────────────────────────────────────
pkg_check_modules(TINYXML2 REQUIRED IMPORTED_TARGET tinyxml2)

# ── Library ───────────────────────────────────────────────────────────────────
add_library(demod_lib STATIC
    src/AppConfig.cpp
    src/IqFetcher.cpp
    src/DemodRouter.cpp
    src/DemodService.cpp
    src/demod/FmDemod.cpp
    src/demod/AmDemod.cpp
    src/demod/FskDemod.cpp
    src/demod/PskQamDemod.cpp
    src/demod/CwDemod.cpp
    src/demod/AfskDemod.cpp
)
target_include_directories(demod_lib PUBLIC include)
target_link_libraries(demod_lib PUBLIC
    sdr_task_api
    PkgConfig::LIQUID
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    PkgConfig::TINYXML2
    Threads::Threads
)
if(TARGET Proton::cpp)
    target_link_libraries(demod_lib PUBLIC Proton::cpp)
elseif(TARGET PkgConfig::PROTON)
    target_link_libraries(demod_lib PUBLIC PkgConfig::PROTON)
else()
    target_link_libraries(demod_lib PUBLIC ${PROTON_CPP_LIB})
    target_include_directories(demod_lib PUBLIC ${PROTON_CPP_INCLUDE_DIR})
endif()
target_compile_options(demod_lib PRIVATE -O2 -march=native)
target_compile_definitions(demod_lib PRIVATE
    SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG)

# ── Binary ────────────────────────────────────────────────────────────────────
add_executable(sdr_demod src/main.cpp)
target_link_libraries(sdr_demod PRIVATE demod_lib)

# ── Install ───────────────────────────────────────────────────────────────────
include(GNUInstallDirs)
install(TARGETS sdr_demod RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES config/demod.xml
        DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/sdr-demod)

# ── Tests ─────────────────────────────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
enable_testing()

add_executable(demod_tests
    tests/test_fm_demod.cpp
    tests/test_am_demod.cpp
    tests/test_fsk_demod.cpp
    tests/test_psk_qam_demod.cpp
    tests/test_cw_demod.cpp
    tests/test_config.cpp
)
target_include_directories(demod_tests PRIVATE tests)
target_link_libraries(demod_tests PRIVATE
    demod_lib
    GTest::gtest_main
)
include(GoogleTest)
gtest_discover_tests(demod_tests)

message(STATUS "┌─ DemodApp ${PROJECT_VERSION} ──────────────────────────────────┐")
message(STATUS "│  C++ standard : C++20                                           │")
message(STATUS "│  liquid-dsp   : ${LIQUID_FOUND}                                 │")
message(STATUS "└─────────────────────────────────────────────────────────────────┘")
