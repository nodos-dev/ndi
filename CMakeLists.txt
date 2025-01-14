set(USE_NOS_SDK_VERSION "1.3.0")
nos_find_sdk(${USE_NOS_SDK_VERSION} NOS_PLUGIN_SDK_TARGET NOS_SUBSYSTEM_SDK_TARGET NOS_SDK_DIR)
if (NOT DEFINED NOS_SDK_DIR)
    message(FATAL_ERROR "Nodos SDK with version ${USE_NOS_SDK_VERSION} not found, please either install it or choose different version")
endif()

set(MODULE_DEPENDENCIES "nos.sys.vulkan-5.8") # List your dependencies here: "module_name-version"
set(dep_idx 0)
foreach(module_name_version ${MODULE_DEPENDENCIES})
    string(REPLACE "-" ";" module_name_version ${module_name_version})
    list(GET module_name_version 0 module_name)
    list(GET module_name_version 1 module_version)
    nos_get_module("${module_name}" "${module_version}" DEP_${dep_idx})
    list(APPEND MODULE_DEPENDENCIES_TARGETS ${DEP_${dep_idx}})
endforeach()
list(APPEND MODULE_DEPENDENCIES_TARGETS Processing.NDI.Lib.x64.lib ${NOS_PLUGIN_SDK_TARGET})

cmake_path(SET FLATC_EXECUTABLE "${NOS_SDK_DIR}/bin/flatc")
nos_generate_flatbuffers("${CMAKE_CURRENT_SOURCE_DIR}/Config" "${CMAKE_CURRENT_SOURCE_DIR}/Source" "cpp" "" NDI_generated)
list(APPEND MODULE_DEPENDENCIES_TARGETS NDI_generated)

link_directories(External/ndi/Lib/x64)

set(INCLUDE_FOLDERS "External/ndi/Include")
nos_add_plugin("nosNDI" "${MODULE_DEPENDENCIES_TARGETS}" "${INCLUDE_FOLDERS}")
nos_group_targets("nosNDI" "NOS Plugins")

# Helpers need C++20
set_target_properties("nosNDI" PROPERTIES CXX_STANDARD 20)