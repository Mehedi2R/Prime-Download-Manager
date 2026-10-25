cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED PDM_VERSION)
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/../VERSION" PDM_VERSION_RAW)
    list(GET PDM_VERSION_RAW 0 PDM_VERSION)
    string(STRIP "${PDM_VERSION}" PDM_VERSION)
endif()

project(PrimeDownloadManager VERSION ${PDM_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt 6 (or Qt 5 fallback)
find_package(Qt6 COMPONENTS Core Gui Widgets Network Concurrent QUIET)
if(NOT Qt6_FOUND)
    find_package(Qt5 5.15 REQUIRED COMPONENTS Core Gui Widgets Network Concurrent)
    set(QT_LIBRARIES Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Network Qt5::Concurrent)
else()
    set(QT_LIBRARIES Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Concurrent)
endif()

if(WIN32)
    add_definitions(-DWIN32_LEAN_AND_MEAN -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS -DQT_NO_DEBUG -DNDEBUG)
    if(MSVC)
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /utf-8 /O2 /GL /Gy /MD /Zc:inline")
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG /OPT:REF /OPT:ICF")
    endif()
endif()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

file(GLOB_RECURSE SRC_CORE "src/core/*.cpp" "src/core/*.h")
file(GLOB_RECURSE SRC_UI "src/ui/*.cpp" "src/ui/*.h")
file(GLOB_RECURSE SRC_IPC "src/ipc/*.cpp" "src/ipc/*.h")
file(GLOB_RECURSE SRC_MEDIA "src/media/*.cpp" "src/media/*.h")

set(SOURCES
    src/main.cpp
    resources/resources.qrc
    resources/version.rc
    ${SRC_CORE}
    ${SRC_UI}
    ${SRC_IPC}
    ${SRC_MEDIA}
)

if(WIN32)
    # Enable Windows subsystem so it launches as a GUI app without a terminal popup (can be overridden with /SUBSYSTEM:CONSOLE for debugging)
    add_executable(${PROJECT_NAME} WIN32 ${SOURCES})
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${QT_LIBRARIES}
        ws2_32
        crypt32
        wininet
        shlwapi
        user32
        shell32
    )
else()
    add_executable(${PROJECT_NAME} ${SOURCES})
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${QT_LIBRARIES}
    )
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    APP_VERSION="${PROJECT_VERSION}"
)

if(WIN32)
    # Ensure embedded media binaries exist (ffmpeg & yt-dlp)
    set(MEDIA_BIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bin")
    file(MAKE_DIRECTORY "${MEDIA_BIN_DIR}")

    if(NOT EXISTS "${MEDIA_BIN_DIR}/ffmpeg.exe")
        if(EXISTS "$ENV{LOCALAPPDATA}/PrimeDownloadManager/bin/ffmpeg.exe")
            file(COPY "$ENV{LOCALAPPDATA}/PrimeDownloadManager/bin/ffmpeg.exe" DESTINATION "${MEDIA_BIN_DIR}")
        else()
            message(STATUS "Downloading standalone ffmpeg.exe...")
            file(DOWNLOAD "https://github.com/eugeneware/ffmpeg-static/releases/download/b4.4/win32-x64"
                "${MEDIA_BIN_DIR}/ffmpeg.exe"
                SHOW_PROGRESS
                TLS_VERIFY ON
            )
        endif()
    endif()

    if(NOT EXISTS "${MEDIA_BIN_DIR}/yt-dlp.exe")
        if(EXISTS "$ENV{LOCALAPPDATA}/PrimeDownloadManager/bin/yt-dlp.exe")
            file(COPY "$ENV{LOCALAPPDATA}/PrimeDownloadManager/bin/yt-dlp.exe" DESTINATION "${MEDIA_BIN_DIR}")
        else()
            message(STATUS "Downloading standalone yt-dlp.exe...")
            file(DOWNLOAD "https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp.exe"
                "${MEDIA_BIN_DIR}/yt-dlp.exe"
                SHOW_PROGRESS
                TLS_VERIFY ON
            )
        endif()
    endif()

    # Find windeployqt
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt
        HINTS
            "${Qt6_DIR}/../../../bin"
            "${QT_DIR}/bin"
            "C:/Qt/6.8.0/msvc2022_64/bin"
    )

    # Standalone zero-dependency deployment POST_BUILD step (windeployqt, MSVC CRT, and embedded tools)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DCONFIG=$<CONFIG>
            -DTARGET_DIR=$<TARGET_FILE_DIR:${PROJECT_NAME}>
            -DTARGET_FILE=$<TARGET_FILE:${PROJECT_NAME}>
            -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -DWINDEPLOYQT=${WINDEPLOYQT_EXECUTABLE}
            -DCXX_COMPILER=${CMAKE_CXX_COMPILER}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Deploy.cmake
        COMMENT "Deploying standalone runtime, MSVC CRT libraries, and media tools..."
    )

    # Inno Setup Installer target
    find_program(ISCC_EXECUTABLE ISCC
        HINTS
            "C:/Users/$ENV{USERNAME}/AppData/Local/Programs/Inno Setup 6"
            "$ENV{LOCALAPPDATA}/Programs/Inno Setup 6"
            "C:/Program Files (x86)/Inno Setup 6"
            "C:/Program Files/Inno Setup 6"
    )

    if(ISCC_EXECUTABLE)
        message(STATUS "Found Inno Setup Compiler: ${ISCC_EXECUTABLE}")
        add_custom_target(installer
            COMMAND ${CMAKE_COMMAND} -E echo "Compiling Inno Setup installer..."
            COMMAND ${ISCC_EXECUTABLE}
                "/DAppBuildDir=$<TARGET_FILE_DIR:${PROJECT_NAME}>"
                "/DMyAppVersion=${PROJECT_VERSION}"
                "${CMAKE_CURRENT_SOURCE_DIR}/../installer.iss"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.."
            DEPENDS ${PROJECT_NAME}
            COMMENT "Building standalone installer executable (dist/PrimeDownloadManager_v${PROJECT_VERSION}.exe)..."
        )
    endif()
endif()

