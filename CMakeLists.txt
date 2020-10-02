cmake_minimum_required(VERSION 3.10)
project(Spacelink C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -g")

# Vulkan and SDL2 (latest, since Vulkan support is fairly recent) are all that needed
find_package(Vulkan)
find_package(SDL2 REQUIRED)

# All source files are located in the VK2D folder
file(GLOB C_FILES src/*.c)
file(GLOB H_FILES include/*.h)
file(GLOB VK2D_FILES Vulkan2D/VK2D/*.c)

# We don't build a library because this engine is intended to have its source
# dropped into a host project (in this case JamEngine) and this whole cmake file
# is just for testing purposes.
include_directories(./include ${SDL2_INCLUDE_DIR} ${Vulkan_INCLUDE_DIRS} ./Vulkan2D)
add_executable(${PROJECT_NAME} main.c ${C_FILES} ${H_FILES} ${VK2D_FILES})
target_link_libraries(${PROJECT_NAME} m ${SDL2_LIBRARIES} ${Vulkan_LIBRARIES})