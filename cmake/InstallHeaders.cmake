# InstallPublicHeaders
#
# INSTALL_PUBLIC_HEADERS([TARGET <target>]
#                        [BASEPATH <basepath>]
#                        [HEADERS] <header1> .. <headern>)
#
# Install a header "<basepath>/include/<headersubpath>" to both of:
#   "${CMAKE_BINARY_DIR}/include/<target>/<headersubpath>" (when cmake runs)
#   "${INSTALL_INCLUDE_DIR}/<target>/<headersubpath>" (during development install)
#
# If headers are supplied with a relative path, "<basepath>" (but not "include")
# is prepended to the header as supplied.
#
# The default for basepath is "${CMAKE_CURRENT_SOURCE_DIR}"
# The default for target is "."
#
function(install_public_headers)

    set(options "")
    set(oneValueArgs TARGET BASEPATH)
    set(multiValueArgs HEADERS)
    cmake_parse_arguments(INSTALL_PUBLIC_HEADERS
            "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN} )
    list(APPEND INSTALL_PUBLIC_HEADERS_HEADERS ${INSTALL_PUBLIC_HEADERS_UNPARSED_ARGUMENTS})

    # Handle unspecified arguments
    if(NOT INSTALL_PUBLIC_HEADERS_TARGET)
        set(INSTALL_PUBLIC_HEADERS_TARGET ".")
    endif()
    if(NOT INSTALL_PUBLIC_HEADERS_HEADERS)
        message(ERROR "No headers specified")
    endif()
    if(NOT INSTALL_PUBLIC_HEADERS_BASEPATH)
        set(INSTALL_PUBLIC_HEADERS_BASEPATH "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()

    # Process and localize each
    foreach(inputPath ${INSTALL_PUBLIC_HEADERS_HEADERS})
        if(NOT IS_ABSOLUTE "${inputPath}")
            set(inputPath "${INSTALL_PUBLIC_HEADERS_BASEPATH}/${inputPath}")
        endif()
        file(RELATIVE_PATH relativePath
                "${INSTALL_PUBLIC_HEADERS_BASEPATH}/include"
                "${inputPath}")

        FILE(GENERATE
                OUTPUT "${CMAKE_BINARY_DIR}/include/${INSTALL_PUBLIC_HEADERS_TARGET}/${relativePath}"
                INPUT "${inputPath}")

        get_filename_component(relativeDir "${relativePath}" DIRECTORY)
        install(FILES "${inputPath}"
                DESTINATION "${INSTALL_INCLUDE_DIR}/${INSTALL_PUBLIC_HEADERS_TARGET}/${relativeDir}"
                COMPONENT dev)
    endforeach()

endfunction(install_public_headers)
