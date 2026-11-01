cmake_minimum_required(VERSION 3.22)

# En Mac: binario universal (Apple Silicon + Intel), desde macOS 11. En Linux/Windows non fai nada.
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "macOS mínimo")
if(NOT CMAKE_OSX_ARCHITECTURES)
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Arquitecturas de Mac")
endif()

project(BarulloADT VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: cartafol irmán ../JUCE, ou o que se indique con -DJUCE_DIR=...
if(NOT JUCE_DIR)
  set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE")
endif()
get_filename_component(JUCE_DIR "${JUCE_DIR}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
if(EXISTS "${JUCE_DIR}/CMakeLists.txt")
  add_subdirectory("${JUCE_DIR}" JUCE)
else()
  include(FetchContent)
  FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.4 GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(JUCE)
endif()

# Núcleo común de Escoita: a versión fixada ou, se existe, a carpeta irmá ../escoita-nucleo
# (para traballar no núcleo e probalo nos plugins á vez); tamén -DESCOITA_NUCLEO=<carpeta>
set(ESCOITA_NUCLEO_VERSION "v0.5.0")
if(NOT ESCOITA_NUCLEO AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../escoita-nucleo/escoita_nucleo")
  set(ESCOITA_NUCLEO "${CMAKE_CURRENT_SOURCE_DIR}/../escoita-nucleo")
endif()
if(ESCOITA_NUCLEO)
  add_subdirectory("${ESCOITA_NUCLEO}" escoita-nucleo)
else()
  include(FetchContent)
  FetchContent_Declare(escoita_nucleo GIT_REPOSITORY https://github.com/callejodev-dotcom/escoita-nucleo.git
                       GIT_TAG ${ESCOITA_NUCLEO_VERSION} GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(escoita_nucleo)
endif()

# Formatos por sistema: Mac → AU, VST3 e app; Windows → VST3 e app; Linux → VST3, LV2 e app
if(APPLE)
  set(FORMATS AU VST3 Standalone)
elseif(WIN32)
  set(FORMATS VST3 Standalone)
else()
  set(FORMATS VST3 LV2 Standalone)
endif()

juce_add_plugin(BarulloADT
  COMPANY_NAME "Escoita"
  COMPANY_WEBSITE "https://www.escoita.link"
  COMPANY_EMAIL "ola@escoita.link"
  BUNDLE_ID "link.escoita.barulloadt"
  PLUGIN_MANUFACTURER_CODE Esco
  PLUGIN_CODE Badt
  PRODUCT_NAME "Barullo ADT"
  DESCRIPTION "Dobre artificial (ADT): a voz dobrada coma en Abbey Road en 1966"
  FORMATS ${FORMATS}
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  VST3_CATEGORIES Fx Modulation
  AU_MAIN_TYPE kAudioUnitType_Effect
  LV2URI "https://www.escoita.link/barulloadt"
  LV2_SHARED_LIBRARY_NAME "BarulloADT"
  COPY_PLUGIN_AFTER_BUILD FALSE)

# CLAP (clap-juce-extensions, licenza MIT): descárgase ao configurar
option(BARULLO_CLAP "Compilar tamén o formato CLAP" ON)
if(BARULLO_CLAP)
  include(FetchContent)
  FetchContent_Declare(clap-juce-extensions
    GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
    GIT_TAG 7adee3a1bd4684d4caa5601100e364abccff4b4d   # main (2026): a 0.26.0 aínda non vai con JUCE 8
    GIT_SUBMODULES_RECURSE TRUE)
  if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)   # FetchContent_Populate + add_subdirectory(... EXCLUDE_FROM_ALL) ata CMake 3.28
  endif()
  FetchContent_GetProperties(clap-juce-extensions)
  if(NOT clap-juce-extensions_POPULATED)
    FetchContent_Populate(clap-juce-extensions)
    add_subdirectory(${clap-juce-extensions_SOURCE_DIR} ${clap-juce-extensions_BINARY_DIR} EXCLUDE_FROM_ALL)
  endif()
  clap_juce_extensions_plugin(TARGET BarulloADT
    CLAP_ID "link.escoita.barulloadt"
    CLAP_FEATURES audio-effect chorus delay stereo
    CLAP_MANUAL_URL "https://www.escoita.link"
    CLAP_SUPPORT_URL "https://www.escoita.link")
endif()

# tipografías Montserrat e Inter, as da familia (licenza OFL, ver Resources/Montserrat-OFL.txt e Inter-OFL.txt)
juce_add_binary_data(BDFonts SOURCES
  Resources/Inter-Medium.ttf
  Resources/Montserrat-SemiBold.ttf
  Resources/Montserrat-Bold.ttf)

# O código propio (o mesmo no plugin e na proba)
set(ADT_SOURCES
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp)

target_sources(BarulloADT PRIVATE ${ADT_SOURCES})
target_include_directories(BarulloADT PRIVATE Source)

# Windows: o código está en UTF-8 (tiles, ñ, ×, ·)
if(MSVC)
  target_compile_options(BarulloADT PRIVATE /utf-8 /bigobj)
endif()

target_compile_definitions(BarulloADT PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(BarulloADT
  PRIVATE
    BDFonts
    juce::juce_audio_utils
    juce::juce_audio_processors
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

escoita_produto(BarulloADT NOME "Barullo ADT" VERSION ${PROJECT_VERSION} REPO callejodev-dotcom/barulloadt)

# Avisos como erros no código propio (non no de JUCE)
option(BARULLO_WERROR "Avisos como erros en Source/ e tools/" OFF)
set(BARULLO_OWN_SOURCES ${ADT_SOURCES} tools/RenderTest.cpp)
if(BARULLO_WERROR)
  if(MSVC)
    set_source_files_properties(${BARULLO_OWN_SOURCES} PROPERTIES COMPILE_OPTIONS "/WX")
  else()
    set_source_files_properties(${BARULLO_OWN_SOURCES} PROPERTIES COMPILE_OPTIONS "-Werror")
  endif()
endif()

# Sanitizadores (memoria e comportamento indefinido) para a proba
option(BARULLO_SANITIZE "Compilar a proba con AddressSanitizer e UBSan" OFF)

# Proba sen interface: a dobre, os presets e o estado; garda un WAV
option(BARULLO_TESTS "Compilar a proba de render" OFF)
if(BARULLO_TESTS)
  juce_add_console_app(BDRenderTest PRODUCT_NAME "BDRenderTest")
  target_sources(BDRenderTest PRIVATE tools/RenderTest.cpp ${ADT_SOURCES})
  target_include_directories(BDRenderTest PRIVATE Source)
  if(MSVC)
    target_compile_options(BDRenderTest PRIVATE /utf-8 /bigobj)
    target_link_options(BDRenderTest PRIVATE /STACK:16777216)
  endif()
  target_compile_definitions(BDRenderTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 BARULLO_NO_PLUGIN_ENTRY=1)
  target_link_libraries(BDRenderTest PRIVATE BDFonts juce::juce_audio_utils juce::juce_audio_processors juce::juce_audio_formats
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
  escoita_produto(BDRenderTest NOME "Barullo ADT" VERSION ${PROJECT_VERSION} REPO callejodev-dotcom/barulloadt PROBA)
  if(BARULLO_SANITIZE AND NOT MSVC)
    target_compile_options(BDRenderTest PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined)
    target_link_options(BDRenderTest PRIVATE -fsanitize=address,undefined)
  endif()
endif()
