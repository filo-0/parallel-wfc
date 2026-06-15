file(GLOB_RECURSE STB_IMAGE_SRC "src/*.cpp")

if(STB_IMAGE_SHARED)
    message("stb-image will be built as a dynamic library")
    add_library(stb-image SHARED ${STB_IMAGE_SRC})
else()
    message("stb-image will be built as a static library")
    add_library(stb-image STATIC ${STB_IMAGE_SRC})
endif()

target_include_directories(stb-image PUBLIC "include")
