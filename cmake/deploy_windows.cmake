# Deploy Windows dependencies alongside opensup_gui.exe
#
# Usage:
#   cmake -P cmake/deploy_windows.cmake
#       -DEXE_PATH=<path/to/opensup_gui.exe>
#       -DSYSROOT=<path/to/mingw/sysroot/mingw>
#       -DDEST_DIR=<output directory>

if(NOT EXISTS "${EXE_PATH}")
    message(FATAL_ERROR "EXE_PATH not found: ${EXE_PATH}")
endif()

set(DEST_DIR "${DEST_DIR}" CACHE PATH "Output directory")
file(MAKE_DIRECTORY "${DEST_DIR}")
file(MAKE_DIRECTORY "${DEST_DIR}/platforms")

# Recursively find all DLL dependencies (non-system)
function(find_dll_deps INPUT_FILE OUT_VAR)
    if(NOT EXISTS "${INPUT_FILE}")
        return()
    endif()

    execute_process(
        COMMAND x86_64-w64-mingw32-objdump -p "${INPUT_FILE}"
        OUTPUT_VARIABLE OBJDUMP_OUT
        ERROR_QUIET
    )

    string(REGEX MATCHALL "DLL Name: ([^\n]+)" MATCHES "${OBJDUMP_OUT}")
    set(RESULT "")

    foreach(MATCH ${MATCHES})
        string(REGEX REPLACE "DLL Name: " "" DLL_NAME "${MATCH}")
        string(STRIP "${DLL_NAME}" DLL_NAME)
        string(TOLOWER "${DLL_NAME}" DLL_LOWER)

        # Skip Windows system DLLs
        set(SYSTEM_DLLS
            kernel32.dll msvcrt.dll ntdll.dll user32.dll gdi32.dll advapi32.dll
            shell32.dll ole32.dll oleaut32.dll comdlg32.dll comctl32.dll
            shlwapi.dll crypt32.dll imm32.dll winspool.drv
            d3d11.dll d3d12.dll d3d9.dll dwrite.dll dxgi.dll dwmapi.dll
            uxtheme.dll setupapi.dll shcore.dll wtsapi32.dll version.dll
            winmm.dll mpr.dll authz.dll netapi32.dll
            api-ms-win-core-synch-l1-2-0.dll
            api-ms-win-crt-*.dll
        )
        set(IS_SYSTEM FALSE)
        foreach(SYS_DLL ${SYSTEM_DLLS})
            if("${DLL_LOWER}" MATCHES "^${SYS_DLL}$")
                set(IS_SYSTEM TRUE)
                break()
            endif()
        endforeach()
        # Skip wildcard api-ms-*
        if("${DLL_LOWER}" MATCHES "^api-ms-")
            set(IS_SYSTEM TRUE)
        endif()

        if(NOT IS_SYSTEM)
            list(APPEND RESULT "${DLL_NAME}")
        endif()
    endforeach()

    set(${OUT_VAR} "${RESULT}" PARENT_SCOPE)
endfunction()

# Recursively collect all unique DLL deps
function(collect_all_deps START_FILE DEPLOY_DIR)
    set(VISITED "")
    set(TO_PROCESS "${START_FILE}")

    while(TO_PROCESS)
        list(POP_FRONT TO_PROCESS CURRENT)

        find_dll_deps("${CURRENT}" DEPS)

        foreach(DLL ${DEPS})
            string(TOLOWER "${DLL}" DLL_LOWER)

            # Find the DLL in the sysroot
            set(FOUND_DLL "")
            if(EXISTS "${SYSROOT}/bin/${DLL}")
                set(FOUND_DLL "${SYSROOT}/bin/${DLL}")
            elseif(EXISTS "${SYSROOT}/lib/qt6/plugins/platforms/${DLL}")
                set(FOUND_DLL "${SYSROOT}/lib/qt6/plugins/platforms/${DLL}")
            endif()

            if(FOUND_DLL AND NOT "${DLL_LOWER}" IN_LIST VISITED)
                list(APPEND VISITED "${DLL_LOWER}")
                list(APPEND TO_PROCESS "${FOUND_DLL}")
                list(APPEND ALL_DEPS "${FOUND_DLL}")
            endif()
        endforeach()
    endwhile()

    set(${OUT_VAR} "${ALL_DEPS}" PARENT_SCOPE)
endfunction()

# Start with the exe
collect_all_deps("${EXE_PATH}" ALL_DLLS)

# Copy all DLLs
foreach(DLL_PATH ${ALL_DLLS})
    get_filename_component(DLL_NAME "${DLL_PATH}" NAME)
    if(DLL_PATH MATCHES "platforms/")
        file(COPY "${DLL_PATH}" DESTINATION "${DEST_DIR}/platforms/")
        message(STATUS "Deployed: platforms/${DLL_NAME}")
    else()
        file(COPY "${DLL_PATH}" DESTINATION "${DEST_DIR}/")
        message(STATUS "Deployed: ${DLL_NAME}")
    endif()
endforeach()

# Also copy the exe
file(COPY "${EXE_PATH}" DESTINATION "${DEST_DIR}/")
message(STATUS "Deployed: opensup_gui.exe")

message(STATUS "\nDeploy complete → ${DEST_DIR}")