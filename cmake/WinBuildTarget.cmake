set(RDF_RC src/rdf.rc) #add resource file when compiling with MSVC 
set(VERSION_LIB Version.lib)
set(LIBRARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/libs) #add libs directory to build dir

# create the targets
set(BINARY_NAME ${CMAKE_PROJECT_NAME})
set(DLL_CORE_NAME ${CMAKE_PROJECT_NAME}Core)
set(DLL_MODULE_NAME ${CMAKE_PROJECT_NAME}Module)

set(LIB_CORE_NAME optimized ${DLL_CORE_NAME}.lib debug ${DLL_CORE_NAME}d.lib)
set(LIB_MODULE_NAME optimized ${DLL_MODULE_NAME}.lib debug ${DLL_MODULE_NAME}d.lib)
set(LIB_NAME optimized ${DLL_GUI_NAME}.lib debug ${DLL_GUI_NAME}d.lib)

#binary
link_directories(${OpenCV_LIBRARY_DIRS} ${LIBRARY_DIR})
set(CHANGLOG_FILE ${CMAKE_CURRENT_SOURCE_DIR}/src/changelog.txt)
add_executable(${BINARY_NAME} WIN32  MACOSX_BUNDLE ${MAIN_SOURCES} ${MAIN_HEADERS} ${RDF_TRANSLATIONS} ${RDF_RC} ${CHANGLOG_FILE}) #changelog is added here, so that i appears in visual studio
set_source_files_properties(${CHANGLOG_FILE} PROPERTIES HEADER_FILE_ONLY TRUE) # define that changelog should not be compiled
target_link_libraries(${BINARY_NAME} ${LIB_CORE_NAME} ${LIB_MODULE_NAME} ${OpenCV_LIBS} ${VERSION_LIB}) 
		
set_target_properties(${BINARY_NAME} PROPERTIES COMPILE_FLAGS "-DNOMINMAX")
set_target_properties(${BINARY_NAME} PROPERTIES LINK_FLAGS_REALLYRELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE}")
set_target_properties(${BINARY_NAME} PROPERTIES LINK_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /SUBSYSTEM:CONSOLE")
set_target_properties(${BINARY_NAME} PROPERTIES LINK_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /SUBSYSTEM:CONSOLE")

# add core
add_library(${DLL_CORE_NAME} SHARED ${CORE_SOURCES} ${CORE_HEADERS} ${RDF_RC})
target_link_libraries(${DLL_CORE_NAME} ${VERSION_LIB} ${OpenCV_LIBS}) 

# add module
add_library(${DLL_MODULE_NAME} SHARED ${MODULE_SOURCES} ${MODULE_HEADERS} ${RDF_RC})
target_link_libraries(${DLL_MODULE_NAME} ${LIB_CORE_NAME} ${OpenCV_LIBS} ${VERSION_LIB}) 

add_dependencies(${DLL_MODULE_NAME} ${DLL_CORE_NAME})
add_dependencies(${BINARY_NAME} ${DLL_MODULE_NAME} ${DLL_CORE_NAME}) 

target_include_directories(${BINARY_NAME} 		PRIVATE ${OpenCV_INCLUDE_DIRS})
target_include_directories(${DLL_MODULE_NAME} 	PRIVATE ${OpenCV_INCLUDE_DIRS})
target_include_directories(${DLL_CORE_NAME} 	PRIVATE ${OpenCV_INCLUDE_DIRS})

qt5_use_modules(${BINARY_NAME} 		Core Network Widgets)
qt5_use_modules(${DLL_MODULE_NAME} 	Core Network Widgets)
qt5_use_modules(${DLL_CORE_NAME} 	Core Network Widgets)

# core flags
set_target_properties(${DLL_CORE_NAME} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_BINARY_DIR}/libs)
set_target_properties(${DLL_CORE_NAME} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_BINARY_DIR}/libs)
set_target_properties(${DLL_CORE_NAME} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY_REALLYRELEASE ${CMAKE_CURRENT_BINARY_DIR}/libs)

set_target_properties(${DLL_CORE_NAME} PROPERTIES COMPILE_FLAGS "-DDLL_CORE_EXPORT -DNOMINMAX")
set_target_properties(${DLL_CORE_NAME} PROPERTIES LINK_FLAGS_REALLYRELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE}")
set_target_properties(${DLL_CORE_NAME} PROPERTIES LINK_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE}")
set_target_properties(${DLL_CORE_NAME} PROPERTIES LINK_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG}")
set_target_properties(${DLL_CORE_NAME} PROPERTIES DEBUG_OUTPUT_NAME ${DLL_CORE_NAME}d)
set_target_properties(${DLL_CORE_NAME} PROPERTIES RELEASE_OUTPUT_NAME ${DLL_CORE_NAME})

# loader flags
set_target_properties(${DLL_MODULE_NAME} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_BINARY_DIR}/libs)
set_target_properties(${DLL_MODULE_NAME} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_BINARY_DIR}/libs)
set_target_properties(${DLL_MODULE_NAME} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY_REALLYRELEASE ${CMAKE_CURRENT_BINARY_DIR}/libs)

set_target_properties(${DLL_MODULE_NAME} PROPERTIES COMPILE_FLAGS "-DDLL_MODULE_EXPORT -DNOMINMAX")
set_target_properties(${DLL_MODULE_NAME} PROPERTIES LINK_FLAGS_REALLYRELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE}")
set_target_properties(${DLL_MODULE_NAME} PROPERTIES LINK_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE}")
set_target_properties(${DLL_MODULE_NAME} PROPERTIES LINK_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG}")
set_target_properties(${DLL_MODULE_NAME} PROPERTIES DEBUG_OUTPUT_NAME ${DLL_MODULE_NAME}d)
set_target_properties(${DLL_MODULE_NAME} PROPERTIES RELEASE_OUTPUT_NAME ${DLL_MODULE_NAME})

SET(CMAKE_SHARED_LINKER_FLAGS_REALLYRELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE}")

# copy required dlls to the directories
set(OpenCV_REQUIRED_MODULES core imgproc FORCE)
foreach(opencvlib ${OpenCV_REQUIRED_MODULES})
	file(GLOB dllpath ${OpenCV_DIR}/bin/Release/opencv_${opencvlib}*.dll)
	file(COPY ${dllpath} DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Release)
	file(COPY ${dllpath} DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/ReallyRelease)
	
	file(GLOB dllpath ${OpenCV_DIR}/bin/Debug/opencv_${opencvlib}*d.dll)
	file(COPY ${dllpath} DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Debug)
endforeach(opencvlib)

set(QTLIBLIST Qt5Core Qt5Gui Qt5Network Qt5Widgets Qt5PrintSupport Qt5Concurrent Qt5Svg)
foreach(qtlib ${QTLIBLIST})
	get_filename_component(QT_DLL_PATH_tmp ${QT_QMAKE_EXECUTABLE} PATH)
	file(COPY ${QT_DLL_PATH_tmp}/${qtlib}.dll DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Release)
	file(COPY ${QT_DLL_PATH_tmp}/${qtlib}.dll DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/ReallyRelease)
	file(COPY ${QT_DLL_PATH_tmp}/${qtlib}d.dll DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Debug)
endforeach(qtlib)

# create settings file for portable version while working
if(NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/ReallyRelease/settings.nfo)
	file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/ReallyRelease/settings.nfo "")
endif()
if(NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/Release/settings.nfo)
	file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/Release/settings.nfo "")
endif()
if(NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/Debug/settings.nfo)
	file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/Debug/settings.nfo "")
endif()

# # copy translation files after each build
# add_custom_command(TARGET ${BINARY_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory \"${CMAKE_CURRENT_BINARY_DIR}/$<CONFIGURATION>/translations/\")
# foreach(QM ${NOMACS_QM})
	# add_custom_command(TARGET ${BINARY_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy \"${QM}\" \"${CMAKE_CURRENT_BINARY_DIR}/$<CONFIGURATION>/translations/\")
# endforeach(QM)

# set properties for Visual Studio Projects
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;ReallyRelease" CACHE STRING "limited configs" FORCE)
add_definitions(/Zc:wchar_t-)
set(CMAKE_CXX_FLAGS_DEBUG "/W4 ${CMAKE_CXX_FLAGS_DEBUG}")
set(CMAKE_CXX_FLAGS_RELEASE "/W4 /O2 ${CMAKE_CXX_FLAGS_RELEASE}")
set(CMAKE_CXX_FLAGS_REALLYRELEASE "${CMAKE_CXX_FLAGS_RELEASE}  /DQT_NO_DEBUG_OUTPUT")
set(CMAKE_EXE_LINKER_FLAGS_REALLYRELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE}")

file(GLOB RDF_AUTOMOC "${CMAKE_BINARY_DIR}/*_automoc.cpp")
source_group("Generated Files" FILES ${RDF_RC} ${RDF_QM} ${RDF_AUTOMOC})
# set_source_files_properties(${NOMACS_TRANSLATIONS} PROPERTIES HEADER_FILE_ONLY TRUE)
# source_group("Translations" FILES ${NOMACS_TRANSLATIONS})
source_group("Changelog" FILES ${CHANGLOG_FILE})

# generate configuration file
if (DLL_CORE_NAME)
	get_property(CORE_DEBUG_NAME TARGET ${DLL_CORE_NAME} PROPERTY DEBUG_OUTPUT_NAME)
	get_property(CORE_RELEASE_NAME TARGET ${DLL_CORE_NAME} PROPERTY RELEASE_OUTPUT_NAME)
	set(RDF_CORE_LIB optimized ${CMAKE_BINARY_DIR}/libs/${CORE_RELEASE_NAME}.lib debug  ${CMAKE_BINARY_DIR}/libs/${CORE_DEBUG_NAME}.lib)
endif()
if(DLL_MODULE_NAME)
	get_property(LOADER_DEBUG_NAME TARGET ${DLL_MODULE_NAME} PROPERTY DEBUG_OUTPUT_NAME)
	get_property(LOADER_RELEASE_NAME TARGET ${DLL_MODULE_NAME} PROPERTY RELEASE_OUTPUT_NAME)
	set(RDF_MODULE_LIB optimized ${CMAKE_BINARY_DIR}/libs/${MODULE_RELEASE_NAME}.lib debug  ${CMAKE_BINARY_DIR}/libs/${MODULE_DEBUG_NAME}.lib)
endif()

set(RDF_LIBS ${RDF_CORE_LIB} ${RDF_MODULE_LIB})
set(RDF_SOURCE_DIR ${CMAKE_SOURCE_DIR})
set(RDF_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/Core ${CMAKE_SOURCE_DIR}/src/Module ${CMAKE_BINARY_DIR})
set(RDF_BUILD_DIRECTORY ${CMAKE_BINARY_DIR})
configure_file(${RDF_SOURCE_DIR}/ReadFramework.cmake.in ${CMAKE_BINARY_DIR}/ReadFrameworkConfig.cmake)