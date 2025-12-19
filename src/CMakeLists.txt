file(GLOB_RECURSE SRC CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/VNEngine.cpp")

add_library(VNEngine STATIC VNEngine.cpp)

target_include_directories(VNEngine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_include_directories(VNEngine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(VNEngine PUBLIC vendor)

target_include_directories(VNEngine
  PUBLIC vendor/sol2/include
)

