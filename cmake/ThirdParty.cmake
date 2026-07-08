set(THIRD_PARTY_DIR ${CMAKE_SOURCE_DIR}/ThirdParty)

# Vulkan SDK is resolved by cmake/SetupVulkanSDK.cmake. Do not add the local
# SDK directory directly here because it is optional and ignored by Git.
if(NOT TARGET Vulkan::Vulkan)
    message(FATAL_ERROR "Vulkan::Vulkan target is missing. Include SetupVulkanSDK before ThirdParty.cmake")
endif()

add_subdirectory(${THIRD_PARTY_DIR}/glfw)

set(HAVE_VULKAN TRUE CACHE BOOL "Enable ImGui Vulkan backend")
add_subdirectory(${THIRD_PARTY_DIR}/imgui)

add_subdirectory(${THIRD_PARTY_DIR}/glm)
add_subdirectory(${THIRD_PARTY_DIR}/tinyobjloader)

set(TINYGLTF_HEADER_ONLY OFF CACHE BOOL "Use tinygltf as header-only" FORCE)
set(TINYGLTF_BUILD_LOADER_EXAMPLE OFF CACHE BOOL "" FORCE)
add_subdirectory(${THIRD_PARTY_DIR}/tinygltf)

add_subdirectory(${THIRD_PARTY_DIR}/spdlog)
add_subdirectory(${THIRD_PARTY_DIR}/stb)
add_subdirectory(${THIRD_PARTY_DIR}/entt)
add_subdirectory(${THIRD_PARTY_DIR}/json)

set(SPIRV_REFLECT_EXECUTABLE OFF CACHE BOOL "" FORCE)
set(SPIRV_REFLECT_STATIC_LIB ON CACHE BOOL "" FORCE)
set(SPIRV_REFLECT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPIRV_REFLECT_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(${THIRD_PARTY_DIR}/SPIRV-Reflect)
