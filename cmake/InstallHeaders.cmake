#
# usage: install_public_headers(library_name ${HEADERS})
#
function(install_public_headers libraryName)
    # First argument is library name, the rest are headers
    if(${ARGC} LESS 2)
        message(ERROR "No headers specified")
    endif()

    # ... so our headers are all the function arguments with the first argument removed
    set(headers ${ARGV})
    list(REMOVE_AT headers 0)

    foreach(header ${headers})

        set(inputPath ${CMAKE_CURRENT_SOURCE_DIR}/${header})
        file(RELATIVE_PATH relativePath ${CMAKE_CURRENT_SOURCE_DIR}/include ${inputPath})
        set(outputPath ${CMAKE_BINARY_DIR}/include/${libraryName}/${relativePath})
        FILE(GENERATE
                OUTPUT ${outputPath}
                INPUT ${inputPath})
    endforeach()

    install(DIRECTORY ${CMAKE_BINARY_DIR}/include/${libraryName}
            DESTINATION  "${INSTALL_INCLUDE_DIR}/DeepCore"
            COMPONENT dev
            )
endfunction(install_public_headers)
