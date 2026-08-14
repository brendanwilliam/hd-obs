cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)
add_subdirectory(deps/libuiohook)

find_package(libobs REQUIRED)
find_package(
  Qt6
  REQUIRED
  COMPONENTS Core Gui Network Svg Widgets
)
find_library(APPLICATION_SERVICES_FRAMEWORK ApplicationServices REQUIRED)
find_library(APPKIT_FRAMEWORK AppKit REQUIRED)

# Qt's 2025-07 prebuilt package still advertises the obsolete AGL framework. macOS 26.5 no longer ships it.
if(APPLE AND TARGET WrapOpenGL::WrapOpenGL)
  set_property(TARGET WrapOpenGL::WrapOpenGL PROPERTY INTERFACE_LINK_LIBRARIES OpenGL::GL)
endif()

target_link_libraries(
  ${CMAKE_PROJECT_NAME}
  PRIVATE
    OBS::libobs
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Svg
    Qt6::Widgets
    uiohook
    ${APPLICATION_SERVICES_FRAMEWORK}
    ${APPKIT_FRAMEWORK}
)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE src deps/libuiohook/include)
target_compile_features(${CMAKE_PROJECT_NAME} PRIVATE cxx_std_17)
if(DEFINED ENV{WEBAPP_URL} AND NOT "$ENV{WEBAPP_URL}" STREQUAL "")
  set(ONLINE_REPORTS_SERVICE_URL "$ENV{WEBAPP_URL}" CACHE STRING "Base URL for the Hands Diff report service" FORCE)
else()
  set(
    ONLINE_REPORTS_SERVICE_URL
    "http://localhost:3000"
    CACHE STRING
    "Base URL for the Hands Diff report service"
    FORCE
  )
endif()
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE ONLINE_REPORTS_SERVICE_URL="${ONLINE_REPORTS_SERVICE_URL}")
target_compile_options(
  ${CMAKE_PROJECT_NAME}
  PRIVATE $<$<CXX_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
)
set_target_properties(
  ${CMAKE_PROJECT_NAME}
  PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF
)

target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE
    src/plugin-main.cpp
    src/hook/uiohook_helper.hpp
    src/hook/uiohook_helper_macos.mm
    src/input/input_data.hpp
    src/input/input_data.cpp
    src/input/input_broker.hpp
    src/input/input_broker.cpp
    src/input/keycodes.h
    src/sources/heatmap/lol_geometry.hpp
    src/sources/heatmap/lol_geometry.cpp
    src/sources/heatmap/lol_settings.hpp
    src/sources/heatmap/lol_settings.cpp
    src/sources/dashboard/capture/lol_capture_switcher.hpp
    src/sources/dashboard/capture/lol_capture_switcher.cpp
    src/sources/dashboard/detection/lol_game_config_watcher.hpp
    src/sources/dashboard/detection/lol_game_config_watcher.cpp
    src/sources/dashboard/detection/lol_input_bindings.hpp
    src/sources/dashboard/detection/lol_input_bindings.cpp
    src/sources/dashboard/detection/lol_game_start_watcher.hpp
    src/sources/dashboard/detection/lol_game_start_watcher.cpp
    src/sources/dashboard/rendering/lol_camera_render.inc
    src/sources/dashboard/rendering/lol_camera_visibility.hpp
    src/sources/dashboard/rendering/lol_camera_visibility.cpp
    src/sources/dashboard/presentation/lol_properties.inc
    src/sources/dashboard/presentation/lol_defaults.inc
    src/sources/dashboard/presentation/lol_dashboard_settings.hpp
    src/sources/dashboard/presentation/lol_dashboard_settings.cpp
    src/sources/dashboard/presentation/lol_dashboard_helpers.hpp
    src/sources/dashboard/rendering/lol_layout.hpp
    src/sources/dashboard/rendering/lol_layout.cpp
    src/sources/dashboard/presentation/lol_source.hpp
    src/sources/dashboard/presentation/lol_source.cpp
    src/sources/dashboard/rendering/lol_visuals.hpp
    src/sources/dashboard/rendering/lol_visuals.cpp
    src/sources/dashboard/rendering/lol_key_labels.hpp
    src/sources/dashboard/rendering/lol_key_labels.cpp
    src/sources/dashboard/rendering/lol_heatmap.cpp
    src/sources/dashboard/rendering/lol_keys.inc
    src/sources/game_report/collection/lol_collector.hpp
    src/sources/game_report/collection/lol_collector.cpp
    src/sources/game_report/collection/lol_game_context.hpp
    src/sources/game_report/collection/lol_game_context.cpp
    src/sources/game_report/collection/lol_gameplay_keys.hpp
    src/sources/game_report/collection/lol_gameplay_keys.cpp
    src/sources/game_report/collection/lol_playback.hpp
    src/sources/game_report/collection/lol_playback.cpp
    src/sources/game_report/collection/lol_v2_metrics.hpp
    src/sources/game_report/collection/lol_v2_metrics.cpp
    src/sources/game_report/collection/lol_shared.inc
    src/sources/game_report/data/lol_diagnostics.hpp
    src/sources/game_report/data/lol_diagnostics.cpp
    src/sources/game_report/data/lol_session_store.hpp
    src/sources/game_report/data/lol_session_store.cpp
    src/sources/game_report/data/lol_types.hpp
    src/sources/game_report/data/lol_types.cpp
    src/sources/game_report/integration/lol_online_reports.hpp
    src/sources/game_report/integration/lol_online_reports.cpp
    src/sources/game_report/presentation/lol_report_manager.hpp
    src/sources/game_report/presentation/lol_report_manager.cpp
    src/sources/hud_layout/lol_layout.hpp
    src/sources/hud_layout/lol_layout.cpp
)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

if(APPLE)
  cmake_path(GET Qt6_DIR PARENT_PATH _qt6_cmake_dir)
  cmake_path(GET _qt6_cmake_dir PARENT_PATH _qt6_lib_dir)
  cmake_path(GET _qt6_lib_dir PARENT_PATH _qt6_root_dir)
  set(_qt_tls_backend "${_qt6_root_dir}/plugins/tls/libqsecuretransportbackend.dylib")
  if(NOT EXISTS "${_qt_tls_backend}")
    message(FATAL_ERROR "Qt Secure Transport TLS backend is missing: ${_qt_tls_backend}")
  endif()
  add_custom_command(
    TARGET ${CMAKE_PROJECT_NAME}
    POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_BUNDLE_DIR:${CMAKE_PROJECT_NAME}>/Contents/Resources/tls"
    COMMAND
      "${CMAKE_COMMAND}" -E copy_if_different "${_qt_tls_backend}"
      "$<TARGET_BUNDLE_DIR:${CMAKE_PROJECT_NAME}>/Contents/Resources/tls/libqsecuretransportbackend.dylib"
    VERBATIM
  )
endif()

include(CTest)
if(BUILD_TESTING)
  add_executable(
    league-safe-area-layout-test
    tests/league_safe_area_layout_test.cpp
    src/sources/hud_layout/lol_layout.cpp
    src/sources/dashboard/rendering/lol_layout.cpp
    src/sources/dashboard/rendering/lol_key_labels.cpp
  )
  target_link_libraries(league-safe-area-layout-test PRIVATE Qt6::Core)
  target_include_directories(league-safe-area-layout-test PRIVATE src deps/libuiohook/include)
  target_compile_features(league-safe-area-layout-test PRIVATE cxx_std_17)
  add_test(NAME league-safe-area-layout COMMAND league-safe-area-layout-test)
  add_executable(
    lol-game-report-test
    tests/lol_game_report_test.cpp
    tests/lol_playback_test.cpp
    src/sources/heatmap/lol_geometry.cpp
    src/sources/game_report/data/lol_types.cpp
    src/sources/game_report/collection/lol_playback.cpp
    src/sources/game_report/data/lol_diagnostics.cpp
    src/sources/game_report/data/lol_session_store.cpp
    src/sources/game_report/integration/lol_online_reports.cpp
  )
  target_link_libraries(lol-game-report-test PRIVATE Qt6::Core Qt6::Gui Qt6::Network Qt6::Widgets)
  target_include_directories(lol-game-report-test PRIVATE src deps/libuiohook/include)
  target_compile_features(lol-game-report-test PRIVATE cxx_std_17)
  add_test(NAME lol-game-report COMMAND lol-game-report-test)
  add_executable(
    lol-session-store-test
    tests/lol_session_store_test.cpp
    src/sources/game_report/data/lol_session_store.cpp
    src/sources/game_report/data/lol_types.cpp
    src/sources/game_report/collection/lol_playback.cpp
  )
  target_link_libraries(lol-session-store-test PRIVATE Qt6::Core Qt6::Gui)
  target_include_directories(lol-session-store-test PRIVATE src)
  target_compile_features(lol-session-store-test PRIVATE cxx_std_17)
  add_test(NAME lol-session-store COMMAND lol-session-store-test)
  add_executable(
    lol-game-context-test
    tests/lol_game_context_test.cpp
    src/sources/game_report/collection/lol_game_context.cpp
  )
  target_link_libraries(lol-game-context-test PRIVATE Qt6::Core)
  target_include_directories(lol-game-context-test PRIVATE src)
  target_compile_features(lol-game-context-test PRIVATE cxx_std_17)
  add_test(NAME lol-game-context COMMAND lol-game-context-test)
  add_executable(
    lol-gameplay-keys-test
    tests/lol_gameplay_keys_test.cpp
    src/sources/game_report/collection/lol_gameplay_keys.cpp
  )
  target_link_libraries(lol-gameplay-keys-test PRIVATE Qt6::Core)
  target_include_directories(lol-gameplay-keys-test PRIVATE src deps/libuiohook/include)
  target_compile_features(lol-gameplay-keys-test PRIVATE cxx_std_17)
  add_test(NAME lol-gameplay-keys COMMAND lol-gameplay-keys-test)
  add_executable(
    lol-v2-metrics-test
    tests/lol_v2_metrics_test.cpp
    src/sources/game_report/collection/lol_v2_metrics.cpp
  )
  target_link_libraries(lol-v2-metrics-test PRIVATE Qt6::Core)
  target_include_directories(lol-v2-metrics-test PRIVATE src)
  target_compile_features(lol-v2-metrics-test PRIVATE cxx_std_17)
  add_test(NAME lol-v2-metrics COMMAND lol-v2-metrics-test)
  add_executable(
    lol-input-bindings-test
    tests/lol_input_bindings_test.cpp
    src/sources/dashboard/detection/lol_input_bindings.cpp
  )
  target_link_libraries(lol-input-bindings-test PRIVATE Qt6::Core)
  target_include_directories(lol-input-bindings-test PRIVATE src)
  target_compile_features(lol-input-bindings-test PRIVATE cxx_std_17)
  add_test(NAME lol-input-bindings COMMAND lol-input-bindings-test)
  foreach(
    test_target
    league-safe-area-layout-test
    lol-game-report-test
    lol-session-store-test
    lol-game-context-test
    lol-gameplay-keys-test
    lol-v2-metrics-test
    lol-input-bindings-test
  )
    target_compile_options(${test_target} PRIVATE -UNDEBUG)
  endforeach()
  if(APPLE)
    target_link_options(league-safe-area-layout-test PRIVATE "-Wl,-rpath,${_qt6_lib_dir}")
    target_link_options(lol-game-report-test PRIVATE "-Wl,-rpath,${_qt6_lib_dir}")
    target_link_options(lol-session-store-test PRIVATE "-Wl,-rpath,${_qt6_lib_dir}")
    target_link_options(lol-game-context-test PRIVATE "-Wl,-rpath,${_qt6_lib_dir}")
    target_link_options(lol-gameplay-keys-test PRIVATE "-Wl,-rpath,${_qt6_lib_dir}")
    target_link_options(lol-v2-metrics-test PRIVATE "-Wl,-rpath,${_qt6_lib_dir}")
    set_target_properties(league-safe-area-layout-test PROPERTIES XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO")
    set_target_properties(lol-game-report-test PROPERTIES XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO")
    set_target_properties(lol-session-store-test PROPERTIES XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO")
    set_target_properties(lol-game-context-test PROPERTIES XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO")
    set_target_properties(lol-gameplay-keys-test PROPERTIES XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO")
    set_target_properties(lol-v2-metrics-test PROPERTIES XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO")
    target_link_options(lol-input-bindings-test PRIVATE "-Wl,-rpath,${_qt6_lib_dir}")
    set_target_properties(lol-input-bindings-test PROPERTIES XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO")
  endif()
endif()
