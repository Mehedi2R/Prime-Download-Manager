# cmake/Deploy.cmake
# Standalone deployment for Prime Download Manager
# Handles windeployqt, MSVC CRT runtime bundling, embedded media tools, and bloat cleanup

if(NOT CONFIG STREQUAL "Release")
    message(STATUS "Skipping standalone deployment for non-Release configuration (${CONFIG})")
    return()
endif()

message(STATUS "==> Running standalone deployment for ${TARGET_FILE}...")

# 1. Run windeployqt
if(WINDEPLOYQT AND EXISTS "${WINDEPLOYQT}")
    message(STATUS "Deploying Qt runtime via ${WINDEPLOYQT}...")
    execute_process(
        COMMAND "${WINDEPLOYQT}" --release --compiler-runtime "${TARGET_FILE}"
        RESULT_VARIABLE res
    )
    if(NOT res EQUAL 0)
        message(WARNING "windeployqt returned exit code: ${res}")
    endif()
endif()

# 2. Bundle MSVC CRT Runtime DLLs (vcruntime140, vcruntime140_1, msvcp140, etc.)
message(STATUS "Deploying MSVC CRT runtime libraries for zero-dependency standalone execution...")
file(GLOB_RECURSE CRT_DLLS LIST_DIRECTORIES false
    "${CXX_COMPILER}/../../../../../../Redist/MSVC/*/x64/Microsoft.VC*.CRT/*.dll"
    "$ENV{VCINSTALLDIR}/Redist/MSVC/*/x64/Microsoft.VC*.CRT/*.dll"
    "C:/Program Files/Microsoft Visual Studio/*/Community/VC/Redist/MSVC/*/x64/Microsoft.VC*.CRT/*.dll"
    "C:/Program Files/Microsoft Visual Studio/*/Professional/VC/Redist/MSVC/*/x64/Microsoft.VC*.CRT/*.dll"
    "C:/Program Files/Microsoft Visual Studio/*/Enterprise/VC/Redist/MSVC/*/x64/Microsoft.VC*.CRT/*.dll"
    "C:/Program Files (x86)/Microsoft Visual Studio/*/Community/VC/Redist/MSVC/*/x64/Microsoft.VC*.CRT/*.dll"
    "C:/Program Files (x86)/Microsoft Visual Studio/*/Professional/VC/Redist/MSVC/*/x64/Microsoft.VC*.CRT/*.dll"
    "C:/Program Files (x86)/Microsoft Visual Studio/*/Enterprise/VC/Redist/MSVC/*/x64/Microsoft.VC*.CRT/*.dll"
)

if(CRT_DLLS)
    list(REMOVE_DUPLICATES CRT_DLLS)
    foreach(dll ${CRT_DLLS})
        get_filename_component(dll_name "${dll}" NAME)
        if(dll_name MATCHES "^(vcruntime|msvcp|concrt|vccorlib)")
            file(COPY "${dll}" DESTINATION "${TARGET_DIR}")
        endif()
    endforeach()
    message(STATUS "Successfully deployed MSVC CRT runtime DLLs to ${TARGET_DIR}")
else()
    message(WARNING "Could not automatically locate MSVC Redistributable CRT directory.")
endif()

# 3. Ensure embedded media tools (ffmpeg.exe and yt-dlp.exe) are copied to bin/
file(MAKE_DIRECTORY "${TARGET_DIR}/bin")
if(EXISTS "${SOURCE_DIR}/bin/ffmpeg.exe")
    file(COPY "${SOURCE_DIR}/bin/ffmpeg.exe" DESTINATION "${TARGET_DIR}/bin")
endif()
if(EXISTS "${SOURCE_DIR}/bin/yt-dlp.exe")
    file(COPY "${SOURCE_DIR}/bin/yt-dlp.exe" DESTINATION "${TARGET_DIR}/bin")
endif()

# 4. Clean unnecessary windeployqt bloat files
set(BLOAT_FILES
    "${TARGET_DIR}/opengl32sw.dll"
    "${TARGET_DIR}/d3dcompiler_47.dll"
    "${TARGET_DIR}/dxcompiler.dll"
    "${TARGET_DIR}/dxil.dll"
    "${TARGET_DIR}/vc_redist.x64.exe"
    "${TARGET_DIR}/tls/qcertonlybackend.dll"
    "${TARGET_DIR}/imageformats/qgif.dll"
    "${TARGET_DIR}/imageformats/qjpeg.dll"
)
foreach(f ${BLOAT_FILES})
    if(EXISTS "${f}")
        file(REMOVE "${f}")
    endif()
endforeach()

set(BLOAT_DIRS
    "${TARGET_DIR}/generic"
    "${TARGET_DIR}/networkinformation"
    "${TARGET_DIR}/styles"
    "${TARGET_DIR}/translations"
)
foreach(d ${BLOAT_DIRS})
    if(EXISTS "${d}")
        file(REMOVE_RECURSE "${d}")
    endif()
endforeach()

message(STATUS "==> Standalone deployment completed successfully.")
