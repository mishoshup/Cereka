add_executable(CerekaGame main.cpp)

target_link_libraries(CerekaGame PRIVATE Cereka)

target_include_directories(CerekaGame PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

if(WIN32)
    set_target_properties(CerekaGame PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/runtimes/windows")
else()
    set_target_properties(CerekaGame PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/runtimes/linux")
endif()
