cmake_minimum_required(VERSION 3.25)
project(AIAssistant VERSION 0.1.0 LANGUAGES CXX RC)

if(NOT WIN32)
  message(FATAL_ERROR "AIAssistant is Windows-only.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(AIASSISTANT_BUILD_UI "Build the web UI with npm as part of the native build" ON)
option(AIASSISTANT_BUILD_TESTS "Build unit tests" ON)
option(AIASSISTANT_BUILD_LIVE_TESTS "Build opt-in subscription smoke tests (uses Luna 6 and Haiku)" OFF)

include(FetchContent)
set(FETCHCONTENT_QUIET ON)

# ---- Dependencies -----------------------------------------------------------
FetchContent_Declare(nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)

# The WebView2 SDK ships as a NuGet package (a zip). We only need its headers and static loader.
FetchContent_Declare(webview2
  URL https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/1.0.4258.31
  DOWNLOAD_NAME webview2.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  SOURCE_SUBDIR _none_)

FetchContent_Declare(doctest
  URL https://github.com/doctest/doctest/archive/refs/tags/v2.5.3.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  SOURCE_SUBDIR _none_)

FetchContent_MakeAvailable(nlohmann_json webview2)

add_library(webview2 INTERFACE)
target_include_directories(webview2 INTERFACE ${webview2_SOURCE_DIR}/build/native/include)
target_link_libraries(webview2 INTERFACE ${webview2_SOURCE_DIR}/build/native/x64/WebView2LoaderStatic.lib)

# ---- Common compile settings -------------------------------------------------
add_library(aiassistant_options INTERFACE)
target_compile_definitions(aiassistant_options INTERFACE
  UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0A00 WINVER=0x0A00)
target_compile_options(aiassistant_options INTERFACE /utf-8 /W4 /permissive- /EHsc /Zc:__cplusplus /MP)

# ---- Core library (no UI; unit-testable) -------------------------------------
add_library(aiassistant_core STATIC
  src/core/Util.cpp
  src/core/Log.cpp
  src/core/Paths.cpp
  src/core/HotkeySpec.cpp
  src/core/Settings.cpp
  src/core/CapturePolicy.cpp
  src/core/ClipboardFormat.cpp
  src/core/UsageBudget.cpp
  src/net/Http.cpp
  src/net/Sse.cpp
  src/platform/Process.cpp
  src/platform/SecretStore.cpp
  src/providers/Catalog.cpp
  src/providers/Wire.cpp
  src/providers/ApiProviders.cpp
  src/providers/CliProviders.cpp
  src/providers/Registry.cpp)
target_sources(aiassistant_core PRIVATE src/providers/Setup.cpp)
target_include_directories(aiassistant_core PUBLIC src)
target_link_libraries(aiassistant_core
  PUBLIC aiassistant_options nlohmann_json::nlohmann_json
  PRIVATE winhttp advapi32 shell32 shlwapi ole32)

# ---- Application -------------------------------------------------------------
add_executable(aiassistant WIN32
  src/main.cpp
  src/app/App.cpp
  src/platform/TrayIcon.cpp
  src/platform/Theme.cpp
  src/platform/Autostart.cpp
  src/platform/ScreenCapture.cpp
  src/ui/WebViewHost.cpp
  src/ui/WebWindow.cpp
  resources/app.rc)
target_link_libraries(aiassistant PRIVATE
  aiassistant_core webview2
  dwmapi windowscodecs comctl32 shell32 shlwapi ole32 oleaut32 uxtheme user32 gdi32 shcore version)
target_include_directories(aiassistant PRIVATE resources)
target_compile_definitions(aiassistant PRIVATE AIASSISTANT_VERSION="${PROJECT_VERSION}")
set_target_properties(aiassistant PROPERTIES OUTPUT_NAME "AIAssistant")
# The manifest is embedded via resources/app.rc.
target_link_options(aiassistant PRIVATE /MANIFEST:NO)

# ---- Web UI ------------------------------------------------------------------
set(UI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ui)
if(AIASSISTANT_BUILD_UI)
  find_program(NPM_EXECUTABLE NAMES npm.cmd npm REQUIRED)
  file(GLOB_RECURSE UI_SOURCES CONFIGURE_DEPENDS
    ${UI_DIR}/src/*)
  file(GLOB UI_ENTRY_POINTS CONFIGURE_DEPENDS ${UI_DIR}/*.html)
  list(APPEND UI_SOURCES ${UI_ENTRY_POINTS} ${UI_DIR}/package.json ${UI_DIR}/vite.config.ts)
  add_custom_command(
    OUTPUT ${UI_DIR}/dist/bar.html
    COMMAND ${CMAKE_COMMAND} -E chdir ${UI_DIR} ${NPM_EXECUTABLE} install --no-audit --no-fund
    COMMAND ${CMAKE_COMMAND} -E chdir ${UI_DIR} ${NPM_EXECUTABLE} run build
    DEPENDS ${UI_SOURCES}
    COMMENT "Building web UI (npm run build)"
    VERBATIM)
  add_custom_target(aiassistant_ui DEPENDS ${UI_DIR}/dist/bar.html)
  add_dependencies(aiassistant aiassistant_ui)
endif()

add_custom_command(TARGET aiassistant POST_BUILD
  COMMAND ${CMAKE_COMMAND} -DSRC=${UI_DIR}/dist -DDST=$<TARGET_FILE_DIR:aiassistant>/ui
          -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CopyUi.cmake
  COMMENT "Copying web UI next to the executable")

# ---- Tests -------------------------------------------------------------------
find_program(POWERSHELL_EXECUTABLE NAMES pwsh powershell REQUIRED)
add_custom_target(aiassistant_installer
  COMMAND ${POWERSHELL_EXECUTABLE} -NoProfile -ExecutionPolicy Bypass
          -File ${CMAKE_CURRENT_SOURCE_DIR}/scripts/build-installer.ps1
          -BuildDir $<TARGET_FILE_DIR:aiassistant>
          -OutputDir ${CMAKE_CURRENT_BINARY_DIR}/installer
          -Version ${PROJECT_VERSION}
  DEPENDS aiassistant
  COMMENT "Packaging Windows setup (requires Inno Setup 6.3+)"
  VERBATIM)

if(AIASSISTANT_BUILD_TESTS)
  FetchContent_MakeAvailable(doctest)
  enable_testing()
  add_executable(aiassistant_tests
    tests/test_main.cpp
    tests/test_sse.cpp
    tests/test_hotkey.cpp
    tests/test_process.cpp
    tests/test_capture_policy.cpp
    tests/test_phase34.cpp
    tests/test_wire.cpp)
  target_sources(aiassistant_tests PRIVATE tests/test_setup.cpp)
  target_include_directories(aiassistant_tests PRIVATE ${doctest_SOURCE_DIR})
  target_link_libraries(aiassistant_tests PRIVATE aiassistant_core)
  add_test(NAME aiassistant_tests COMMAND aiassistant_tests)
endif()

if(AIASSISTANT_BUILD_LIVE_TESTS)
  add_executable(aiassistant_live_tests tests/live_cli.cpp)
  target_link_libraries(aiassistant_live_tests PRIVATE aiassistant_core)
endif()
