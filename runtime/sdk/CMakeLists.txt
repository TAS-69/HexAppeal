cmake_minimum_required(VERSION 3.20)
project(HexAppealWardrobeSDK LANGUAGES CXX)
add_library(HexAppealWardrobeSDK INTERFACE)
target_include_directories(HexAppealWardrobeSDK INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(HexAppealWardrobeSDK INTERFACE cxx_std_17)
option(HEX_APPEAL_BUILD_EXAMPLE "Compile the provider integration example" OFF)
if(HEX_APPEAL_BUILD_EXAMPLE)
  if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "The provider integration example targets x64 Windows")
  endif()
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/ProviderDefinition.hpp
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_provider.py
      ${CMAKE_CURRENT_SOURCE_DIR}/examples/outfit.json --output ${CMAKE_CURRENT_BINARY_DIR}/ProviderDefinition.hpp --force
    DEPENDS tools/generate_provider.py examples/outfit.json VERBATIM)
  add_library(HexAppealProviderExample OBJECT examples/NativeOutfitProvider.cpp ${CMAKE_CURRENT_BINARY_DIR}/ProviderDefinition.hpp)
  target_include_directories(HexAppealProviderExample PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
  target_link_libraries(HexAppealProviderExample PRIVATE HexAppealWardrobeSDK)
  add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/PartsDefinition.hpp
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_provider.py
      ${CMAKE_CURRENT_SOURCE_DIR}/examples/parts-outfit.json --output ${CMAKE_CURRENT_BINARY_DIR}/PartsDefinition.hpp --force
    DEPENDS tools/generate_provider.py examples/parts-outfit.json VERBATIM)
  add_library(HexAppealPartsExample OBJECT examples/PartsOutfitProvider.cpp ${CMAKE_CURRENT_BINARY_DIR}/PartsDefinition.hpp)
  target_include_directories(HexAppealPartsExample PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
  target_link_libraries(HexAppealPartsExample PRIVATE HexAppealWardrobeSDK)
  foreach(example IN ITEMS Advanced Renderer)
    if(example STREQUAL "Advanced")
      set(definition advanced-outfit.json)
      set(source AdvancedOutfitProvider.cpp)
    else()
      set(definition renderer-outfit.json)
      set(source CustomRendererProvider.cpp)
    endif()
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${example}Definition.hpp
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_provider.py
        ${CMAKE_CURRENT_SOURCE_DIR}/examples/${definition} --output ${CMAKE_CURRENT_BINARY_DIR}/${example}Definition.hpp --force
      DEPENDS tools/generate_provider.py examples/${definition} VERBATIM)
    add_library(HexAppeal${example}Example OBJECT examples/${source} ${CMAKE_CURRENT_BINARY_DIR}/${example}Definition.hpp)
    target_include_directories(HexAppeal${example}Example PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    target_link_libraries(HexAppeal${example}Example PRIVATE HexAppealWardrobeSDK)
  endforeach()
endif()
