find_package(Qt4 4.8 REQUIRED)
include(${QT_USE_FILE})

find_package(qtExtensions REQUIRED)
include(${qtExtensions_USE_FILE})

find_package(VTK 6.2 REQUIRED)
include(${VTK_USE_FILE})

include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${MAPTK_SOURCE_DIR}
  ${MAPTK_BINARY_DIR}
)

set(gui_am_ui
  CameraView.ui
  MainWindow.ui
  WorldView.ui
)

set(gui_ui
  ${gui_am_ui}
  CameraOptions.ui
  FeatureOptions.ui
)

set(gui_resources
  icons/icons.qrc
)

set(gui_moc_headers
  ActorColorButton.h
  CameraOptions.h
  CameraView.h
  FeatureOptions.h
  MainWindow.h
  WorldView.h
)

set(gui_sources
  ActorColorButton.cxx
  CameraOptions.cxx
  CameraView.cxx
  FeatureOptions.cxx
  Project.cxx
  MainWindow.cxx
  WorldView.cxx
  main.cxx
  vtkMaptkCamera.cxx
  vtkMaptkCameraRepresentation.cxx
)

qt4_wrap_ui(gui_ui_sources ${gui_ui})
qt4_wrap_cpp(gui_moc_sources ${gui_moc_headers})
qt4_add_resources(gui_res_sources ${gui_resources})

qte_amc_wrap_ui(gui_amc_sources ActionManagerDialog ${gui_am_ui})

maptk_add_executable(mapgui
  ${gui_sources}
  ${gui_ui_sources}
  ${gui_moc_sources}
  ${gui_res_sources}
  ${gui_amc_sources}
)

target_link_libraries(mapgui
  maptk
  vtkGUISupportQt
  vtkInteractionStyle
  vtkViewsCore
  vtkRenderingFreeTypeOpenGL
  vtkRenderingVolumeOpenGL
  vtkRenderingOpenGL
  vtkRenderingCore
  vtkCommonCore
  vtksys
  qtExtensions
  ${QT_LIBRARIES}
)