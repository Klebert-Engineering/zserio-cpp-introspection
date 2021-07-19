cmake_minimum_required(VERSION 3.10.2)
project(zsr-runtime)

include(FetchContent)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
set(gtest_force_shared_crt ON CACHE BOOL "Always use msvcrt.dll")

set(TEST "${CMAKE_CURRENT_SOURCE_DIR}/test")

define_property(GLOBAL PROPERTY ZSERIO_DEPENDS_PY
    BRIEF_DOCS "Zserio dependency grepper."
    FULL_DOCS "Greps all external dependencies of a zserio top-level sub-package.")
set_property(GLOBAL PROPERTY ZSERIO_DEPENDS_PY "${CMAKE_CURRENT_SOURCE_DIR}/submodule-depends.py")

if (NOT MSVC)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON) # This does not set -fPIC :(
  set(CMAKE_CXX_FLAGS -fPIC)
endif()

## Dependencies
####

if (ZSR_BUILD_TESTS AND NOT TARGET Catch2)
  FetchContent_Declare(Catch2
    GIT_REPOSITORY "https://github.com/catchorg/Catch2.git"
    GIT_TAG        "v2.13.6"
    GIT_SHALLOW    ON)
  FetchContent_MakeAvailable(Catch2)
endif()
if (TARGET Catch2)
  get_target_property(CATCH2_SOURCE_DIR Catch2 SOURCE_DIR)
  list(APPEND CMAKE_MODULE_PATH "${CATCH2_SOURCE_DIR}/contrib")
endif()

if (NOT TARGET stx)
  FetchContent_Declare(stx
    GIT_REPOSITORY "https://github.com/Klebert-Engineering/stx.git"
    GIT_TAG        "main"
    GIT_SHALLOW    ON)
  FetchContent_MakeAvailable(stx)
endif()

if (NOT TARGET speedyj)
  FetchContent_Declare(speedyj
    GIT_REPOSITORY "https://github.com/Klebert-Engineering/speedyj.git"
    GIT_TAG        "master"
    GIT_SHALLOW    ON)
  FetchContent_MakeAvailable(speedyj)
endif()

## ZSERIO C++ runtime (optional)
####

if (NOT TARGET ZserioCppRuntime)
  option(ZSERIO_RT_DIR "Directory of zserio C++ runtime CMakeLists.txt (e.g. extensions/cpp/runtime/src)")
  if (ZSERIO_RT_DIR)
    add_subdirectory("${ZSERIO_RT_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/zserio-cpp-rt")
  else ()
    message(FATAL_ERROR "Option ZSERIO_RT_DIR is empty.")
  endif()
endif()

## ZSR Runtime
####

add_library(zsr SHARED
  src/types.cpp
  src/types-json.cpp
  src/error.cpp
  src/introspectable.cpp
  src/introspectable-json.cpp
  src/getset.cpp

  zsr/error.hpp
  zsr/export.hpp
  zsr/find.hpp
  zsr/introspectable.hpp
  zsr/introspectable-json.hpp
  zsr/introspectable-private.hpp
  zsr/getset.hpp
  zsr/reflection-main.hpp
  zsr/types.hpp
  zsr/types-json.hpp
  zsr/variant.hpp)

target_compile_features(zsr
  PUBLIC cxx_std_17)

target_include_directories(zsr
  PUBLIC
    "${SRC}")

target_link_libraries(zsr
  PUBLIC
    ZserioCppRuntime
    speedyj::speedyj
    stx::stx)

set_target_properties(zsr
  PROPERTIES
    src_dir "${CMAKE_CURRENT_SOURCE_DIR}")

target_compile_definitions(zsr
  PRIVATE
    ZSR_BUILD=1)

target_compile_options(zsr
  PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -fvisibility=hidden>)

function (submodule_dependencies ROOT TOP_LEVEL_PKG MAX_DEPTH RESULT)
  get_property(depends_py GLOBAL PROPERTY ZSERIO_DEPENDS_PY)
  find_package(Python3 COMPONENTS Interpreter)

  if (NOT Python3_FOUND)
    message(FATAL_ERROR "Could not find python3 interpreter!")
  endif()

  execute_process(
    COMMAND
    "${Python3_EXECUTABLE}" "${depends_py}" "${ROOT}" "${TOP_LEVEL_PKG}" "${MAX_DEPTH}"
    OUTPUT_VARIABLE DEPS_OUTPUT
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  set(${RESULT} "${DEPS_OUTPUT}" PARENT_SCOPE)
endfunction()

function (add_submodule_lib_recursive target top_level_pkg zs_root gen_src_root parent_dir module_name max_depth)
  _add_submodule_lib_recursive("${target}" "${top_level_pkg}" "${zs_root}" "${gen_src_root}" "${parent_dir}" "${module_name}" "${max_depth}" 0)
endfunction()

function (_add_submodule_lib_recursive target top_level_pkg zs_root gen_src_root parent_dir module_name max_depth depth)
  # Append parent dir name to module name
  get_filename_component(parent_dir_name "${parent_dir}" NAME)
  if (parent_dir_name)
    if (module_name)
      set(module_name "${module_name}--${parent_dir_name}")
    else()
      set(module_name "${parent_dir_name}")
    endif()
  endif()

  # Check recursion depth
  math(EXPR depth "${depth} + 1")
  if (depth GREATER max_depth)
    add_submodule_lib("${target}" "${top_level_pkg}" "${zs_root}" "${gen_src_root}" "${parent_dir}" "${module_name}" "${max_depth}")
    return()
  endif()

  # Glob subdirs recursive, create module lib if files exist
  file(GLOB subdirs LIST_DIRECTORIES TRUE "${parent_dir}/*")
  foreach (subdir IN LISTS subdirs)
    if (IS_DIRECTORY "${subdir}")
      file(RELATIVE_PATH rel_subdir "${gen_src_root}" "${subdir}")
      message(STATUS "Scanning ${rel_subdir}")

      _add_submodule_lib_recursive("${target}" "${top_level_pkg}" "${zs_root}" "${gen_src_root}" "${subdir}" "${module_name}" "${max_depth}" "${depth}")
    endif()
  endforeach()
endfunction()

function (add_submodule_lib target top_level_pkg zs_root gen_src_root subdir module_name max_depth)
  set(module_name "zsr--${module_name}")

  file(GLOB_RECURSE subdir_sources "${subdir}/*.cpp")
  file(GLOB_RECURSE subdir_headers "${subdir}/*.h" "${subdir}/*.hpp")

  if (subdir_sources OR subdir_headers)
    message("Added target ${module_name}")

    file(RELATIVE_PATH rel_subdir "${gen_src_root}" "${subdir}")
    submodule_dependencies("${zs_root}/../${rel_subdir}" "${top_level_pkg}" "${max_depth}" dependencies)

    list(SORT dependencies)
    list(REMOVE_DUPLICATES dependencies)
    list(REMOVE_ITEM dependencies "${module_name}")
    message("Dependencies for ${module_name}: ${dependencies}")

    if (subdir_sources)

      add_library(${module_name} SHARED
        ${subdir_sources}
        ${subdir_headers})

      target_include_directories(${module_name}
        PUBLIC "${gen_src_root}")
      target_link_libraries(${module_name}
        PUBLIC ZserioCppRuntime ${dependencies})

      set_target_properties(${module_name}
        PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS ON)

      if (MSVC)
        target_compile_options(${module_name}
          PUBLIC
          /bigobj
          /EHsc
          /wd4100)
      else()
        target_compile_options(${module_name}
          PUBLIC
          -fPIC)
      endif()

    else() # No sources, only headers

      message("Target ${module_name} is header-only.")
      add_library(${module_name} INTERFACE)
      target_include_directories(${module_name}
        INTERFACE "${gen_src_root}")

    endif()

    target_link_libraries(${target}
      INTERFACE ${module_name})
  endif()
endfunction()

function (add_zserio_module ZSR_MODULE_NAME)
  cmake_parse_arguments(PARSE_ARGV 0
    ZSR_MODULE "EXCLUDE_FROM_ALL" "ROOT;ENTRY;TOP_LEVEL_PKG;SUBDIR_DEPTH" "")

  if (NOT ZSR_MODULE_ROOT)
    message(FATAL_ERROR "Missing zserio-module argument ROOT!")
  elseif (NOT ZSR_MODULE_ENTRY)
    message(FATAL_ERROR "Missing zserio-module argument ENTRY!")
  elseif (NOT ZSR_MODULE_SUBDIR_DEPTH)
    message(WARNING "add_zserio_module: Parameter SUBDIR_DEPTH is not set. Defaulting to 0.")
    set(ZSR_MODULE_SUBDIR_DEPTH 0)
  endif()

  set(GEN_SRC_ROOT "${CMAKE_CURRENT_BINARY_DIR}/${ZSR_MODULE_NAME}.zserio-gen/")
  file(REMOVE_RECURSE "${GEN_SRC_ROOT}")
  file(MAKE_DIRECTORY "${GEN_SRC_ROOT}")

  set(GEN_REFLECT_SRC_ROOT "${CMAKE_CURRENT_BINARY_DIR}/${ZSR_MODULE_NAME}.zserio-gen/reflection/")
  file(MAKE_DIRECTORY "${GEN_REFLECT_SRC_ROOT}")

  find_program(JAVA java)
  if (NOT JAVA)
    message(FATAL_ERROR "Could not find java!")
  endif()

  set(ZSERIO ${JAVA})

  get_target_property(JAR_PATH zserio jar)
  if (${JAR_PATH} EQUAL "NOTFOUND")
    message(FATAL_ERROR "Could not read property 'jar' from target 'zserio'.")
  endif()

  list(APPEND ZSERIO "-jar")
  list(APPEND ZSERIO "${JAR_PATH}")

  set(zserio_top_level_arg "")
  if (ZSR_MODULE_TOP_LEVEL_PKG)
    set(zserio_top_level_arg "-setTopLevelPackage")
  endif()

  execute_process(
    COMMAND ${ZSERIO} ${zserio_top_level_arg} ${ZSR_MODULE_TOP_LEVEL_PKG}
            -cpp ${GEN_SRC_ROOT}
            -cpp_reflect ${GEN_REFLECT_SRC_ROOT}
            -src ${ZSR_MODULE_ROOT}
            ${ZSR_MODULE_ENTRY}
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RESULT_VARIABLE ZSERIO_RESULT)
  if (NOT ZSERIO_RESULT EQUAL "0")
      message(FATAL_ERROR "/////////// zserio FAILED to generate ${ZSR_MODULE_NAME} module! ///////////")
  endif()

  # Create single cmake interface library to link all sub-libs
  add_library(${ZSR_MODULE_NAME} INTERFACE)

  # Create sub-libs recursive per directory
  message("Scanning zsr library subdirectories")
  add_submodule_lib_recursive(
    ${ZSR_MODULE_NAME}
    ${ZSR_MODULE_TOP_LEVEL_PKG}
    ${ZSR_MODULE_ROOT}
    ${GEN_SRC_ROOT}
    "${GEN_SRC_ROOT}/${ZSR_MODULE_TOP_LEVEL_PKG}"
    FALSE
    ${ZSR_MODULE_SUBDIR_DEPTH})

  target_include_directories(${ZSR_MODULE_NAME}
    INTERFACE "${GEN_SRC_ROOT}")

  # Reflection runtime lib
  get_target_property(ZSR_SRC zsr src_dir)

  file(GLOB_RECURSE package_sources "${GEN_REFLECT_SRC_ROOT}/*.cpp")

  add_library(${ZSR_MODULE_NAME}-reflection SHARED
    "${package_sources}"
    "${GEN_REFLECT_SRC_ROOT}/reflection-main.cpp")

  target_compile_definitions(${ZSR_MODULE_NAME}-reflection
    PRIVATE
      ZSR_BUILD=1)

  target_compile_options(${ZSR_MODULE_NAME}-reflection
    PRIVATE
      $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
      $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -fvisibility=hidden>)

  target_include_directories(${ZSR_MODULE_NAME}-reflection
    PRIVATE
      "${ZSR_SRC}/src")

  target_link_libraries(${ZSR_MODULE_NAME}-reflection
    zsr
    ${ZSR_MODULE_NAME})
endfunction()

## Test
####

include(CTest)
include(Catch)

function(add_zsr_test TARGET FILE)
  add_executable(${TARGET}
    "test/main.cpp"
    "${FILE}")

  target_include_directories(${TARGET}
    PRIVATE "${TEST}/zserio")

  target_link_libraries(${TARGET}
    PRIVATE zsr Catch2::Catch2)

  target_compile_options(${TARGET}
    PRIVATE
      $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
      $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall>)

  target_compile_features(${TARGET}
    PUBLIC cxx_std_17)

  catch_discover_tests(${TARGET})
endfunction()

function(add_zsr_module_test NAME)
  cmake_parse_arguments(PARSE_ARGV 1
    ZSR_TEST "" "TOP_LEVEL_PKG" "")

  add_zserio_module(zs-${NAME}
    ROOT "${TEST}/zserio/${NAME}"
    ENTRY "${NAME}.zs"
    TOP_LEVEL_PKG "${ZSR_TEST_TOP_LEVEL_PKG}"
    SUBDIR_DEPTH 1)
  add_zsr_test(${NAME} "${TEST}/zserio/${NAME}/${NAME}.cpp")

  target_link_libraries(${NAME}
    PRIVATE zs-${NAME} zs-${NAME}-reflection)
endfunction()

if (ZSR_BUILD_TESTS)
  add_zsr_test(variant-test "${TEST}/variant-test.cpp")
  add_zsr_module_test(bitmask_test TOP_LEVEL_PKG zsr)
  add_zsr_module_test(choice_test TOP_LEVEL_PKG zsr)
  add_zsr_module_test(union_test TOP_LEVEL_PKG zsr)
  add_zsr_module_test(enumeration_test TOP_LEVEL_PKG zsr)
  add_zsr_module_test(constant_test TOP_LEVEL_PKG zsr)
  add_zsr_module_test(structure_test TOP_LEVEL_PKG zsr)
  add_zsr_module_test(service_test TOP_LEVEL_PKG zsr)
  add_zsr_module_test(find_test TOP_LEVEL_PKG zsr)
  #add_zsr_module_test(json_test TOP_LEVEL_PKG zsr)
  add_zsr_module_test(getset_test TOP_LEVEL_PKG zsr)
endif()
