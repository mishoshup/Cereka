file(GLOB_RECURSE SRC CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_library(VisualNovelEngine STATIC ${SRC})

target_include_directories(VisualNovelEngine
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(VisualNovelEngine PUBLIC vendor)

target_include_directories(VisualNovelEngine
  PUBLIC vendor/sol2/include
)
