cmake_minimum_required(VERSION 3.0)
set(PROJ atik_control)
set(MINOR_VERSION "0")
set(MID_VERSION "1")
set(MAJOR_VERSION "0")
set(PROJ_VERSION "${MAJOR_VERSION}.${MID_VERSION}.${MINOR_VERSION}")

project(${PROJ} VERSION ${PROJ_VERSION} LANGUAGES C CXX)

# default flags
set(CMAKE_C_FLAGS_RELEASE "")
set(CMAKE_C_FLAGS_DEBUG "")
set(CMAKE_CXX_FLAGS_RELEASE "")
set(CMAKE_CXX_FLAGS_DEBUG "")
set(CMAKE_C_FLAGS "-O2 -std=gnu99")
set(CMAKE_CXX_FLAGS "-O2")

set(CMAKE_COLOR_MAKEFILE ON)

# here is one of two variants: all .c in directory or .c files in list
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} SOURCES)
#set(SOURCES list_of_c_files)

# cmake -DEBUG=1 -> debugging
if(DEFINED EBUG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wextra -Wall -Werror -W")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wextra -Wall -Werror -W")
    set(CMAKE_BUILD_TYPE DEBUG)
    add_definitions(-DEBUG) # -DATIK_DEBUG=1)
else()
    set(CMAKE_BUILD_TYPE RELEASE)
endif()

if(DEFINED CMAKE_INSTALL_PREFIX AND CMAKE_INSTALL_PREFIX MATCHES "/usr/local")
    set(CMAKE_INSTALL_PREFIX "/usr")
endif()

if(DEFINED USE_BTA AND USE_BTA STREQUAL "yes")
    add_definitions(-DUSE_BTA)
endif()
if(DEFINED TELALT)
    add_definitions(-DTELALT=${TELALT})
endif()
if(DEFINED TELLONG)
    add_definitions(-DTELLONG=${TELLONG})
endif()
if(DEFINED TELLAT)
    add_definitions(-DTELLAT=${TELLAT})
endif()
if(DEFINED TELFOCUS)
    add_definitions(-DTELFOCUS=${TELFOCUS})
endif()

find_package(PkgConfig REQUIRED)
#set(MODULES cfitsio>=3.0)

# find CFITSIO
SET(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR})
FIND_PACKAGE(CFITSIO REQUIRED)

# additional modules on condition
if(DEFINED USE_PNG AND USE_PNG STREQUAL "yes")
    set(MODULES ${MODULES} libpng>=1.2)
    add_definitions(-DUSEPNG)
endif()
pkg_check_modules(${PROJ} REQUIRED ${MODULES})
if(DEFINED USE_RAW AND USE_RAW STREQUAL "yes")
    add_definitions(-DUSERAW)
endif()

# change wrong behaviour with install prefix
#if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT AND CMAKE_INSTALL_PREFIX MATCHES "/usr/local")
#    message("Change default install path to /usr")
#    set(CMAKE_INSTALL_PREFIX "/usr")
#endif()
message("Install dir prefix: ${CMAKE_INSTALL_PREFIX}")

# directory should contain dir locale/ru for gettext translations
set(LCPATH ${CMAKE_SOURCE_DIR}/locale/ru)

if(NOT DEFINED LOCALEDIR)
    if(DEFINED DEBUG)
        set(LOCALEDIR ${CMAKE_CURRENT_SOURCE_DIR}/locale)
    else()
        set(LOCALEDIR ${CMAKE_INSTALL_PREFIX}/share/locale)
    endif()
endif()

# gettext files
set(PO_FILE ${LCPATH}/messages.po)
set(MO_FILE ${LCPATH}/LC_MESSAGES/${PROJ}.mo)
set(RU_FILE ${LCPATH}/ru.po)
set(CTAGS_FILE ${CMAKE_SOURCE_DIR}/${PROJ}.c.tags)

# exe file
add_executable(${PROJ} ${SOURCES} ${MO_FILE})
target_link_libraries(${PROJ} ${${PROJ}_LIBRARIES} -latikccd)
include_directories(${${PROJ}_INCLUDE_DIRS})
link_directories(${${PROJ}_LIBRARY_DIRS} )
add_definitions(${CFLAGS} -DLOCALEDIR=\"${LOCALEDIR}\"
        -DPACKAGE_VERSION=\"${VERSION}\" -DGETTEXT_PACKAGE=\"${PROJ}\"
        -DMINOR_VERSION=\"${MINOR_VERSION}\" -DMID_VERSION=\"${MID_VERSION}\"
        -DMAJOR_VERSION=\"${MAJOR_VESION}\")

# Installation of the program
if(NOT DEFINED DEBUG)
    INSTALL(FILES ${MO_FILE} DESTINATION "share/locale/ru/LC_MESSAGES")
    INSTALL(TARGETS ${PROJ} DESTINATION "bin")
else()
    install(CODE "MESSAGE(\"Don't install in DEBUG mode! First run cmake without -DEBUG defined.\")")
endif(NOT DEFINED DEBUG)

###### gettext & ctags ######
if(DEFINED EBUG)
    message("Generate locale & tags files")
    find_package(Gettext REQUIRED)
    find_program(GETTEXT_XGETTEXT_EXECUTABLE xgettext)
    if(NOT GETTEXT_XGETTEXT_EXECUTABLE OR NOT GETTEXT_MSGFMT_EXECUTABLE)
        message(FATAL_ERROR "xgettext not found")
    endif()
    file(MAKE_DIRECTORY ${LCPATH})
    file(MAKE_DIRECTORY ${LCPATH}/LC_MESSAGES)

    add_custom_command(
        OUTPUT ${PO_FILE}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMAND ${GETTEXT_XGETTEXT_EXECUTABLE} --from-code=utf-8 ${SOURCES} -c -k_ -kN_ -o ${PO_FILE}
        COMMAND sed -i 's/charset=.*\\\\n/charset=koi8-r\\\\n/' ${PO_FILE}
        COMMAND enconv ${PO_FILE}
        DEPENDS ${SOURCES}
    )
    # we need this to prevent ru.po & .mo from deleting by make clean
    add_custom_target(
        RU_FILE
        COMMAND [ -f ${RU_FILE} ] && ${GETTEXT_MSGMERGE_EXECUTABLE} -Uis ${RU_FILE} ${PO_FILE} || cp ${PO_FILE} ${RU_FILE}
        DEPENDS ${PO_FILE} ${SOURCES}
    )
    add_custom_target(
        MO_FILE
        COMMAND make RU_FILE && ${GETTEXT_MSGFMT_EXECUTABLE} ${RU_FILE} -o ${MO_FILE}
        DEPENDS ${SOURCES}
    )
    add_dependencies(${PROJ} MO_FILE)

    set(CFLAGS_A CFLAGS=${CFLAGS})
    add_custom_target(ctags
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMAND CFLAGS=-DEBUG geany -g ${CTAGS_FILE} ${SOURCES}
        DEPENDS ${SOURCES}
    )
    add_dependencies(${PROJ} ctags)
endif()
