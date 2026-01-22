cmake_minimum_required(VERSION 3.16)
project(dicom_viewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------- Qt ----------
# Suporta Qt6 e Qt5
find_package(Qt6 COMPONENTS Widgets OpenGL REQUIRED QUIET)
if(Qt6_FOUND)
  set(QT_MAJOR 6)
  set(QT_LIBS Qt6::Widgets Qt6::OpenGL)
else()
  find_package(Qt5 COMPONENTS Widgets OpenGL REQUIRED)
  set(QT_MAJOR 5)
  set(QT_LIBS Qt5::Widgets Qt5::OpenGL)
endif()

# ---------- VTK ----------
# Modulos necessarios para:
# - widget Qt + OpenGL
# - leitura DICOM
# - viewer 2D
set(VTK_COMPONENTS
  CommonCore
  CommonDataModel
  ImagingCore
  IOImage
  RenderingCore
  RenderingOpenGL2
  InteractionStyle
  InteractionImage
  GUISupportQt
)
# Suprime warning de cross-compiling do VTK compile tools
set(VTK_COMPILE_TOOLS_IGNORE_CROSS_COMPILING_WARNING ON CACHE BOOL "" FORCE)
set(VTK_COMPILE_TOOLS_FIND_QUIETLY ON CACHE BOOL "" FORCE)
set(vtkcompiletools_FIND_QUIETLY ON CACHE BOOL "" FORCE)
find_package(VTK QUIET REQUIRED COMPONENTS ${VTK_COMPONENTS})

# GDCM para fallback de leitura de DICOM comprimido
find_package(GDCM CONFIG)

# VTK moderno: usar targets, sem VTK_USE_FILE (evita aviso deprecado)
set(VTK_LIBS)
foreach(comp ${VTK_COMPONENTS})
  list(APPEND VTK_LIBS "VTK::${comp}")
endforeach()

# Opcional: suporte a GDCM (DICOM comprimido) se o módulo estiver disponível
if (TARGET VTK::IOGDCM)
  list(APPEND VTK_LIBS VTK::IOGDCM)
  set(HAS_VTK_IOGDCM TRUE)
endif()

add_executable(dicom_viewer
  src/main.cpp
  src/MainWindow.h
  src/MainWindow.cpp
)

target_include_directories(dicom_viewer PRIVATE src)
target_link_libraries(dicom_viewer PRIVATE ${QT_LIBS} ${VTK_LIBS})
if (GDCM_FOUND)
  target_link_libraries(dicom_viewer PRIVATE gdcmMSFF)
  target_compile_definitions(dicom_viewer PRIVATE HAS_GDCM_FALLBACK)
endif()

if (HAS_VTK_IOGDCM)
  target_compile_definitions(dicom_viewer PRIVATE HAS_VTK_IOGDCM)
endif()

# Autoinit para VTK (necessario em varios setups, especialmente no Windows)
if (VTK_VERSION VERSION_GREATER_EQUAL "9.0.0")
  vtk_module_autoinit(
    TARGETS dicom_viewer
    MODULES ${VTK_LIBS}
  )
endif()

# Qt MOC (para sinais/slots)
set_target_properties(dicom_viewer PROPERTIES
  AUTOMOC ON
  AUTOUIC OFF
  AUTORCC OFF
)

if (QT_MAJOR EQUAL 6)
  target_compile_definitions(dicom_viewer PRIVATE QT6_BUILD)
endif()
