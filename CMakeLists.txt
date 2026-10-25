cmake_minimum_required(VERSION 3.16)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" PDM_VERSION_RAW)
list(GET PDM_VERSION_RAW 0 PDM_VERSION)
string(STRIP "${PDM_VERSION}" PDM_VERSION)

project(PrimeDownloadManagerRoot VERSION ${PDM_VERSION} LANGUAGES CXX)

add_subdirectory(src-qt)
