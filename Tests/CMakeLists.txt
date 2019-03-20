
# madronalib/tests/CMakeLists.txt
# CMake file for madronalib project tests.

link_libraries(madronalib)

if (BUILD_SHARED_LIBS)
    add_definitions(-DMADRONALIB_DLL)
    link_libraries("${OPENGL_gl_LIBRARY}" "${MATH_LIBRARY}")
else()
    link_libraries(${madronalib_LIBRARIES})
endif()

include_directories("${MADRONALIB_SOURCE_DIR}/include"
                    "${MADRONALIB_SOURCE_DIR}/deps")

if ("${OPENGL_INCLUDE_DIR}")
    include_directories("${OPENGL_INCLUDE_DIR}")
endif()


#--------------------------------------------------------------------
# Add include Directories
#--------------------------------------------------------------------

include_directories("${CMAKE_SOURCE_DIR}/source/app")
include_directories("${CMAKE_SOURCE_DIR}/source/DSP")
include_directories("${CMAKE_SOURCE_DIR}/source/matrix")
include_directories("${CMAKE_SOURCE_DIR}/source/networking")
include_directories("${CMAKE_SOURCE_DIR}/source/procs")

# external libraries
include_directories("${CMAKE_SOURCE_DIR}/source/external/aes256")
include_directories("${CMAKE_SOURCE_DIR}/source/external/cJSON")
include_directories("${CMAKE_SOURCE_DIR}/source/external/ffft")

include_directories("${CMAKE_SOURCE_DIR}/source/external/oscpack/ip")
include_directories("${CMAKE_SOURCE_DIR}/source/external/oscpack/osc")
include_directories("${CMAKE_SOURCE_DIR}/source/external/oscpack/zeroconf")

include_directories("${CMAKE_SOURCE_DIR}/external/utf")

if ("${OPENGL_INCLUDE_DIR}")
    include_directories("${OPENGL_INCLUDE_DIR}")
endif()

#--------------------------------------------------------------------
# Compiler flags
#--------------------------------------------------------------------

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

#--------------------------------------------------------------------
# Add all the tests.
#--------------------------------------------------------------------



message ("adding tests..." )



add_executable(tests 
	catch.hpp 
	dspOpsTest.cpp
	OSCTest.cpp
	procsTest.cpp
	queueTest.cpp
	resourceMapTest.cpp 
	dspBufferTest.cpp 
	symbolTest.cpp
	timerTest.cpp
	tests.cpp
	tests.h 
	)

