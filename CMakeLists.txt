cmake_minimum_required(VERSION 3.16)

project(CloudBreadMuisc_V1 VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets LinguistTools Network WebSockets Sql WebEngineWidgets Multimedia MultimediaWidgets Core5Compat HttpServer Concurrent)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets LinguistTools Network WebSockets Sql WebEngineWidgets Multimedia MultimediaWidgets Core5Compat HttpServer Concurrent)
find_package(OpenSSL REQUIRED)

set(TAGLIB_ROOT "${PROJECT_SOURCE_DIR}/taglib-install")

include_directories(${TAGLIB_ROOT}/include)
link_directories(${TAGLIB_ROOT}/lib)

find_library(TAGLIB_LIBRARY NAMES tag PATHS ${TAGLIB_ROOT}/lib)
if(NOT TAGLIB_LIBRARY)
    message(FATAL_ERROR "taglib library not found!")
endif()

find_library(LIBHV_LIBRARY NAMES hv PATHS ${PROJECT_SOURCE_DIR}/lib)
if(NOT LIBHV_LIBRARY)
    message(FATAL_ERROR "libhv library not found!")
endif()

FILE(GLOB ORIGIN *.cpp *.h)
FILE(GLOB MODULE module/*.cpp module/*.h module/*.hpp)
FILE(GLOB VIEW view/*.h view/*.cpp)
FILE(GLOB QRCODE qrcode/*.hpp qrcode/*.cpp)
FILE(GLOB MODELVIEW ModelView/*.h ModelView/*.cpp)
FILE(GLOB QQMUSICDECODER QQMusicDecoder/*.h QQMusicDecoder/*.cpp)
FILE(GLOB BILIWEBAPI BiliWebAPI/*.h BiliWebAPI/*.cpp)

source_group(Module FILES ${MODULE})
source_group(View FILES ${VIEW})
source_group(QRCode FILES ${QRCODE})
source_group(ModelView FILES ${MODELVIEW})
source_group(BiliWebAPI FILES ${BILIWEBAPI})

set(PROJECT_SOURCES
    ${ORIGIN}
    ${MODULE}
    ${VIEW}
    ${QRCODE}
    ${MODELVIEW}
    ${BILIWEBAPI}
)

include_directories(
    include
    module
    view
    qrcode
    ModelView
    BiliWebAPI
)

set(RESOURCE_FILES resources.qrc)

# 管理员权限相关配置 - 修正版
if(WIN32)
    # 方法1: 使用链接器选项请求管理员权限，避免资源冲突
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MANIFESTUAC:\"level='requireAdministrator' uiAccess='false'\"")

    # 方法2: 如果方法1不工作，可以尝试创建自定义清单并禁用Qt的自动清单
    # 取消下面的注释并注释掉上面的方法1
    # set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} /MANIFEST:NO")
    # set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MANIFEST:NO")
    # set(APP_MANIFEST_FILE "${CMAKE_CURRENT_SOURCE_DIR}/app.manifest")
    # if(NOT EXISTS ${APP_MANIFEST_FILE})
    #     file(WRITE ${APP_MANIFEST_FILE} "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>
    # <assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">
    #     <trustInfo xmlns=\"urn:schemas-microsoft-com:asm.v3\">
    #         <security>
    #             <requestedPrivileges>
    #                 <requestedExecutionLevel level=\"requireAdministrator\" uiAccess=\"false\" />
    #             </requestedPrivileges>
    #         </security>
    #     </trustInfo>
    # </assembly>")
    # endif()
    # set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MANIFEST:EMBED /MANIFESTINPUT:${APP_MANIFEST_FILE}")
elseif(APPLE)
    # macOS: 配置权限相关设置
    set_target_properties(${PROJECT_NAME} PROPERTIES
        XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME YES
        XCODE_ATTRIBUTE_OTHER_CODE_SIGN_FLAGS "--deep"
    )
endif()

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
qt_add_executable(CloudBreadMuisc_V1
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        res.rc
        resources.qrc
        include/ElaWidgetTools.qrc
    )
else()
    if(ANDROID)
        add_library(CloudBreadMuisc_V1 SHARED
            ${PROJECT_SOURCES}
        )
    else()
        add_executable(CloudBreadMuisc_V1
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(CloudBreadMuisc_V1 PRIVATE
  Qt${QT_VERSION_MAJOR}::Widgets
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/elawidgettools.lib
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/hv.lib
  ${TAGLIB_ROOT}/lib/tag.lib
  Qt${QT_VERSION_MAJOR}::Network
  Qt${QT_VERSION_MAJOR}::WebSockets
  Qt${QT_VERSION_MAJOR}::Sql
  Qt${QT_VERSION_MAJOR}::WebEngineWidgets
  Qt${QT_VERSION_MAJOR}::Concurrent
  Qt${QT_VERSION_MAJOR}::Multimedia
  Qt${QT_VERSION_MAJOR}::MultimediaWidgets
  Qt${QT_VERSION_MAJOR}::Core5Compat
  Qt${QT_VERSION_MAJOR}::HttpServer
  OpenSSL::Crypto
  OpenSSL::SSL
)

if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.CloudBreadMuisc_V1)
endif()
set_target_properties(CloudBreadMuisc_V1 PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS CloudBreadMuisc_V1
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(CloudBreadMuisc_V1)
endif()

if(WIN32)
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy
      "${TAGLIB_ROOT}/bin/tag.dll"
      "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${PROJECT_SOURCE_DIR}/lib/hv.dll"
      $<TARGET_FILE_DIR:${PROJECT_NAME}>)
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${PROJECT_SOURCE_DIR}/lib/ElaWidgetTools.dll"
      $<TARGET_FILE_DIR:${PROJECT_NAME}>)
endif()
