# - Try to find Tox
# Once done this will define
#  LIBTOXAV_FOUND - System has Tox
#  LIBTOXAV_INCLUDE_DIRS - The Tox include directories
#  LIBTOXAV_LIBRARIES - The libraries needed to use Tox
#  LIBTOXAV_DEFINITIONS - Compiler switches required for using Tox

find_package(PkgConfig)

pkg_check_modules(PKG_LIBTOXAV REQUIRED libtoxav)

set(LIBTOXAV_DEFINITIONS ${PKG_LIBTOXAV_CFLAGS_OTHER})

find_path(LIBTOXAV_INCLUDE_DIR tox/toxav.h HINTS
    ${PKG_LIBTOXAV_INCLUDEDIR}
    ${PKG_LIBTOXAV_INCLUDE_DIRS}
)

find_library(LIBTOXAV_LIBRARY NAMES toxav HINTS
    ${PKG_LIBTOXAV_LIBDIR}
    ${PKG_LIBTOXAV_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
# Handle the QUIETLY and REQUIRED arguments and set LIBTOXAV_FOUND to TRUE
# if all listed variables are TRUE.
find_package_handle_standard_args(
    libtoxav
    DEFAULT_MSG
    LIBTOXAV_LIBRARY
    LIBTOXAV_INCLUDE_DIR
)

mark_as_advanced(LIBTOXAV_INCLUDE_DIR LIBTOXAV_LIBRARY)

set(LIBTOXAV_LIBRARIES ${LIBTOXAV_LIBRARY})
set(LIBTOXAV_INCLUDE_DIRS ${LIBTOXAV_INCLUDE_DIR})
