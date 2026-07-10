function(loupe_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} INTERFACE /W4 /permissive- /EHsc)
    else()
        target_compile_options(${target} INTERFACE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
        )
    endif()
endfunction()
