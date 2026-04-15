set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../vendor/imgui")

set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdlrenderer3.cpp
)

add_executable(CerekaLauncher main.cpp ${IMGUI_SOURCES})

target_include_directories(CerekaLauncher PRIVATE
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/../vendor/SDL/include
    ${CMAKE_BINARY_DIR}/../vendor/SDL/include-revision
)

target_link_libraries(CerekaLauncher PRIVATE SDL3::SDL3)

target_compile_features(CerekaLauncher PRIVATE cxx_std_17)

set_target_properties(CerekaLauncher PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
