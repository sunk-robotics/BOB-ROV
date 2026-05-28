# FindLibDataChannel.cmake
# Finds the libdatachannel library, either from a system install or a
# previous ament_vendor build.
#
# Output variables:
#   LibDataChannel_FOUND          - True if found
#   LibDataChannel_INCLUDE_DIRS   - Include directories
#   LibDataChannel_LIBRARIES      - Libraries to link against
#
# Imported target:
#   LibDataChannel::LibDataChannel

# Try config-file package first (libdatachannel installs one)
find_package(LibDataChannel CONFIG QUIET)
if(LibDataChannel_FOUND)
    message(STATUS "Found LibDataChannel from installed config in ${LibDataChannel_DIR}")
    return()
endif()

# Fall back to manual header/library search
find_path(LibDataChannel_INCLUDE_DIR
    NAMES rtc/rtc.hpp
    PATH_SUFFIXES include
)

find_library(LibDataChannel_LIBRARY
    NAMES datadatachannel
)

mark_as_advanced(LibDataChannel_INCLUDE_DIR LibDataChannel_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibDataChannel
    DEFAULT_MSG
    LibDataChannel_LIBRARY
    LibDataChannel_INCLUDE_DIR
)

if(LibDataChannel_FOUND)
    set(LibDataChannel_INCLUDE_DIRS ${LibDataChannel_INCLUDE_DIR})
    set(LibDataChannel_LIBRARIES    ${LibDataChannel_LIBRARY})

    if(NOT TARGET LibDataChannel::LibDataChannel)
        add_library(LibDataChannel::LibDataChannel UNKNOWN IMPORTED)
        set_target_properties(LibDataChannel::LibDataChannel PROPERTIES
            IMPORTED_LOCATION         ${LibDataChannel_LIBRARY}
            INTERFACE_INCLUDE_DIRECTORIES ${LibDataChannel_INCLUDE_DIR}
        )
    endif()
endif()
