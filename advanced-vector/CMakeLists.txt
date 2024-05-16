CMAKE_MINIMUM_REQUIRED(VERSION 3.15.0)

set(PROJECT AdvancedVector)
project(${PROJECT})
set(CMAKE_CXX_STANDARD 17)

file(GLOB SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp" 
	              "${CMAKE_CURRENT_SOURCE_DIR}/*.h" )

source_group ("sources" FILES ${SOURCES} )

add_executable(${PROJECT} ${SOURCES} )


