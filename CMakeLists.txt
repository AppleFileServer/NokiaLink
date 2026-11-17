cmake_minimum_required(VERSION 3.15)

project(NokiaLink VERSION 0.1.0)

# Built with Retro68 for the Macintosh Plus, as MacMusic, MacTrack and
# MacMoney are:
#   cmake -S . -B build \
#     -DCMAKE_TOOLCHAIN_FILE=$HOME/Retro68-build/build-target/cmake/retro68.toolchain.cmake
#   cmake --build build
# The portable modules are tested on the modern Mac by tests/run-host.sh,
# which doesn't use this file.

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_C_FLAGS_RELEASE "-O2")

# The version shown is the edition's letter, then the number: "B-0.1.0"
# is the British edition (programme decision P22). The letter is not the
# release stage; the stage (development, alpha, beta or release) goes in
# the vers resources only. The edition also sets their region code. The
# version is set only here and above.
set(EDITION "B")
set(VERS_STAGE "development")

if(EDITION STREQUAL "B")
    set(VERS_REGION 2)                  # verBritain
else()
    message(FATAL_ERROR "No region code for edition ${EDITION}")
endif()
set(SHOWN_VERSION "${EDITION}-${PROJECT_VERSION}")

# CMake writes them into version.h (for the code: the About box) and
# version.r (the vers and signature resources), both in the build folder.
# vers holds the number in BCD (major, then minor and bug-fix).
set(patch "${PROJECT_VERSION_PATCH}")
if(patch STREQUAL "")
    set(patch 0)
endif()
math(EXPR VERS_MAJOR
     "(${PROJECT_VERSION_MAJOR} / 10) * 16 + ${PROJECT_VERSION_MAJOR} % 10"
     OUTPUT_FORMAT HEXADECIMAL)
math(EXPR VERS_MINOR "${PROJECT_VERSION_MINOR} * 16 + ${patch}"
     OUTPUT_FORMAT HEXADECIMAL)
configure_file(src/app/version.h.in ${CMAKE_BINARY_DIR}/version.h @ONLY)
configure_file(resources/version.r.in ${CMAKE_BINARY_DIR}/version.r @ONLY)

# Creator NkLk (PROVISIONAL: SPECIFICATION.md section 7). The portable
# modules (src/core: the FBUS link; src/store: dates, numbers, the rules
# and contact cards) aren't all used by the application yet; they are
# built in so that they are compiled for the 68000 each time.
add_application(NokiaLink
    CREATOR "NkLk"
    src/app/main.c
    src/core/fbus.c
    src/core/link.c
    src/core/nlcal.c
    src/core/nlpbk.c
    src/core/nlgsm.c
    src/core/nlsms.c
    src/core/nlinfo.c
    src/core/nlsync.c
    src/core/nlbase.c
    src/core/nlctsync.c
    src/store/nldate.c
    src/store/nlnumber.c
    src/store/nlusage.c
    src/store/nlvcard.c
    src/store/nltext.c
    src/store/nlcrc.c
    src/store/nlcontact.c
    resources/NokiaLink.r
    ${CMAKE_BINARY_DIR}/version.r
)
target_include_directories(NokiaLink PRIVATE ${CMAKE_BINARY_DIR} src/app src/core src/store)
target_compile_options(NokiaLink PRIVATE -Wall -Wextra -Wno-multichar -Wno-unused-parameter -Wno-pointer-sign)
