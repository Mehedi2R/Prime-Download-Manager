cmake_minimum_required(VERSION 3.16)

project(PrimeDownloadManager VERSION 3.0.46 LANGUAGES CXX)

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

