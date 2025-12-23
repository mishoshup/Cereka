file(GLOB_RECURSE SRC CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_library(Cereka STATIC ${SRC})

target_include_directories(Cereka
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_include_directories(Cereka
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(Cereka PUBLIC vendor)

target_include_directories(Cereka
  PUBLIC vendor/sol2/include
)

