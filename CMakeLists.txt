cmake_minimum_required(VERSION 3.21)
project(OweraMindFlow VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Svg SvgWidgets PrintSupport Test)

# ---- Core library (model + layout + canvas + io), shared by app and tests ----
add_library(mindflow_core STATIC
  src/model/Node.cpp
  src/model/NodeStyle.cpp
  src/model/Theme.cpp
  src/model/Connection.cpp
  src/model/Document.cpp
  src/model/Commands.cpp
  src/layout/LayoutEngine.cpp
  src/canvas/NodeItem.cpp
  src/canvas/BranchItem.cpp
  src/canvas/ConnectionItem.cpp
  src/canvas/MindMapView.cpp
  src/outline/OutlineView.cpp
  src/io/DocumentStore.cpp
  src/io/Exporters.cpp
  src/io/Importers.cpp
)
target_include_directories(mindflow_core PUBLIC src)
target_link_libraries(mindflow_core PUBLIC
  Qt6::Widgets Qt6::Svg Qt6::SvgWidgets Qt6::PrintSupport)

# ---- Application ----
add_executable(mindflow
  src/app/main.cpp
  src/app/MainWindow.cpp
  src/app/Inspector.cpp
  resources/resources.qrc
)
target_link_libraries(mindflow PRIVATE mindflow_core)

# ---- Render harness (visual verification) ----
add_executable(mindflow_render_sample tools/render_sample.cpp)
target_link_libraries(mindflow_render_sample PRIVATE mindflow_core)

# ---- UX walkthrough harness (drives the real MainWindow) ----
add_executable(mindflow_uxtest
  tools/uxtest.cpp
  src/app/MainWindow.cpp
  src/app/Inspector.cpp
  resources/resources.qrc
)
target_link_libraries(mindflow_uxtest PRIVATE mindflow_core Qt6::Test)

add_executable(mindflow_uxkeys
  tools/uxkeys.cpp
  src/app/MainWindow.cpp
  src/app/Inspector.cpp
  resources/resources.qrc
)
target_link_libraries(mindflow_uxkeys PRIVATE mindflow_core Qt6::Test)

# ---- Tests ----
enable_testing()
add_executable(mindflow_tests tests/test_model.cpp)
target_link_libraries(mindflow_tests PRIVATE mindflow_core Qt6::Test)
add_test(NAME mindflow_tests COMMAND mindflow_tests)

# ---- Install (used by Flatpak / AppImage / distro packaging) ----
include(GNUInstallDirs)
install(TARGETS mindflow RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES packaging/com.owera.MindFlow.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES packaging/com.owera.MindFlow.metainfo.xml
        DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
install(FILES resources/mindflow.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
        RENAME com.owera.MindFlow.svg)

add_executable(mindflow_uxkeys2
  tools/uxkeys2.cpp
  src/app/MainWindow.cpp
  src/app/Inspector.cpp
  resources/resources.qrc
)
target_link_libraries(mindflow_uxkeys2 PRIVATE mindflow_core Qt6::Test)

add_executable(mindflow_uxrepro
  tools/uxrepro.cpp
  src/app/MainWindow.cpp
  src/app/Inspector.cpp
  resources/resources.qrc
)
target_link_libraries(mindflow_uxrepro PRIVATE mindflow_core Qt6::Test)

add_executable(mindflow_gifframes tools/gifframes.cpp)
target_link_libraries(mindflow_gifframes PRIVATE mindflow_core)

add_executable(mindflow_uxshortcuts
  tools/uxshortcuts.cpp
  src/app/MainWindow.cpp
  src/app/Inspector.cpp
  resources/resources.qrc
)
target_link_libraries(mindflow_uxshortcuts PRIVATE mindflow_core Qt6::Test)
