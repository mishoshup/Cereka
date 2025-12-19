file(GLOB_RECURSE SRC CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
# Define the executable
add_library(VisualNovelEngine STATIC ${SRC})

target_include_directories(VisualNovelEngine
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Link the vendor library (propagates SDL3 + SDL_image)
target_link_libraries(VisualNovelEngine PRIVATE vendor)

target_include_directories(VisualNovelEngine
    PRIVATE vendor/sol2/include
)
