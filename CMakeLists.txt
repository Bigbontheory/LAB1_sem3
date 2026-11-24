cmake_minimum_required(VERSION 4.3)
project(LAB_1)

set(CMAKE_CXX_STANDARD 20)

add_executable(lab_benchmark benchmarks/benchmark.cpp)
target_include_directories(lab_benchmark PRIVATE include)

include(FetchContent)

option(BUILD_GUI "Build the Dear ImGui interface" ON)
if(BUILD_GUI)
    FetchContent_Declare(glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
    )
    FetchContent_Declare(imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.91.9b
    )
    set(GLFW_BUILD_DOCS OFF)
    set(GLFW_BUILD_TESTS OFF)
    set(GLFW_BUILD_EXAMPLES OFF)
    FetchContent_MakeAvailable(glfw imgui)
    find_package(OpenGL REQUIRED)

    add_library(imgui_backend STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
        ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
    )
    target_include_directories(imgui_backend PUBLIC
        ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends
    )
    target_link_libraries(imgui_backend PUBLIC glfw OpenGL::GL)

    set(benchmark_csv "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/timings.csv")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${benchmark_csv}")
    file(READ "${benchmark_csv}" benchmark_data)
    configure_file(gui/BenchmarkData.hpp.in generated/BenchmarkData.hpp @ONLY)
    add_executable(LAB_1 gui/App.cpp gui/Benchmarks.cpp gui/TreeDiagram.cpp)
    target_include_directories(LAB_1 PRIVATE include "${CMAKE_CURRENT_BINARY_DIR}/generated")
    target_link_libraries(LAB_1 PRIVATE imgui_backend)
endif()

FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.15.2
)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()
add_executable(AVLTreeTests tests/AVLTreeTests.cpp)
target_include_directories(AVLTreeTests PRIVATE include/AVLTree include/LAB2_sem2)
target_link_libraries(AVLTreeTests PRIVATE GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(AVLTreeTests)

add_executable(SetTests tests/SetTests.cpp)
target_include_directories(SetTests PRIVATE include)
target_link_libraries(SetTests PRIVATE GTest::gtest_main)
gtest_discover_tests(SetTests)

add_executable(StressTests tests/StressTests.cpp)
target_include_directories(StressTests PRIVATE include)
target_link_libraries(StressTests PRIVATE GTest::gtest_main)
gtest_discover_tests(StressTests)
