# madronalib/source/CMakeLists.txt


#--------------------------------------------------------------------
# Add include Directories
#--------------------------------------------------------------------

include_directories(app)
include_directories(DSP)
include_directories(matrix)
include_directories(networking)
include_directories(procs)

# external libraries
include_directories(external/aes256)
include_directories(external/cJSON)
include_directories(external/ffft)

include_directories(external/oscpack/ip)
include_directories(external/oscpack/osc)
include_directories(external/oscpack/zeroconf)

include_directories(external/utf)


#--------------------------------------------------------------------
# add sources
#--------------------------------------------------------------------

# TODO ml-juce Timer integration
#    add_definitions(-D_MADRONALIB_TIMERS_USE_JUCE)


macro(add_source_dir DIR)
  file(GLOB CPP_SOURCES "${DIR}/*.cpp")
  file(GLOB C_SOURCES "${DIR}/*.c")
  list(APPEND madronalib_SOURCES "${CPP_SOURCES}")
  list(APPEND madronalib_SOURCES "${C_SOURCES}")
endmacro()

macro(add_header_dir DIR)
  file(GLOB H_SOURCES "${DIR}/*.h")
  list(APPEND madronalib_SOURCES "${H_SOURCES}")
endmacro()

set(madronalib_SOURCES "")

add_source_dir("app")
add_source_dir("matrix")
add_source_dir("networking")
add_source_dir("procs")

add_source_dir("external/aes256")
add_source_dir("external/cJSON")

# add headers. just for viewing files in the IDE - is there a better way?
add_header_dir("app")
add_header_dir("matrix")
add_header_dir("networking")
add_header_dir("procs")


# send binary output to the current build/bin
# set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)




#--------------------------------------------------------------------
# add oscpack as source
#--------------------------------------------------------------------


add_source_dir("external/oscpack/ip")
add_source_dir("external/oscpack/osc")
add_source_dir("external/oscpack/zeroconf")

# separate versions of NetworkingUtils.cpp and UdpSocket.cpp are provided for Win32 and POSIX
# the IpSystemTypePath selects the correct ones based on the current platform

# TODO oscpack for Windows

if(WIN32)
 set(IpSystemTypePath ip/win32)
 set(LIBS ${LIBS} Ws2_32 winmm)

 # TODO

else(WIN32)
  set(IpSystemTypePath external/oscpack/ip/posix)
endif(WIN32)

set(IP_SRC "${IpSystemTypePath}/NetworkingUtils.cpp" "${IpSystemTypePath}/UdpSocket.cpp")

list(APPEND madronalib_SOURCES "${IP_SRC}")

if(MSVC)
  # Force to always compile with W4
  if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
    string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
  endif()
elseif(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
  # Update if necessary
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-long-long -pedantic")
endif()


# TODO add platform-specific oscpack source

# TEMP print sources
# message ("madronalib sources:" ${madronalib_SOURCES})


#--------------------------------------------------------------------
# set source groups for source above
#--------------------------------------------------------------------

source_group(dsp REGULAR_EXPRESSION "DSP/.*")
source_group(app REGULAR_EXPRESSION "app/.*")
source_group(matrix REGULAR_EXPRESSION "matrix/.*")
source_group(networking REGULAR_EXPRESSION "networking/.*")
source_group(procs REGULAR_EXPRESSION "procs/.*")

#--------------------------------------------------------------------
# create and install libraries and headers
#--------------------------------------------------------------------

add_library(madronalib STATIC ${madronalib_SOURCES})
set_target_properties(madronalib PROPERTIES
                      OUTPUT_NAME "madrona"
                      VERSION 0.9
                      POSITION_INDEPENDENT_CODE ON
                      FOLDER "madronalib")

include(GNUInstallDirs)
install(
    TARGETS madronalib
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# top-level includes
install(FILES ../include/mldsp.h DESTINATION include/madronalib)
install(FILES ../include/madronalib.h DESTINATION include/madronalib)

install(FILES external/cJSON/cJSON.h DESTINATION include/madronalib)

file(GLOB APP_HDR "app/*.h")
file(GLOB DSP_HDR "DSP/*.h")
file(GLOB MTX_HDR "matrix/*.h")
file(GLOB NTW_HDR "networking/*.h")
file(GLOB AES_HDR "external/aes256/*.h")

install(FILES 
    ${APP_HDR}  
    ${DSP_HDR}   
    ${MTX_HDR}   
    ${NTW_HDR}   
    ${AES_HDR}   
    DESTINATION include/madronalib)

if(APPLE)
    install(FILES ${NETWORKING_HEADERS} DESTINATION include/madronalib)

    # TEMP install oscpack headers publicly
    # TODO encapsulate oscpack / rewrite networking
    
    file(GLOB OSCPACK1 "external/oscpack/ip/*.h")
    install(FILES ${OSCPACK1} DESTINATION include/madronalib/oscpack)

    file(GLOB OSCPACK2 "external/oscpack/osc/*.h")
    install(FILES ${OSCPACK2} DESTINATION include/madronalib/oscpack)

    file(GLOB OSCPACK3 "external/oscpack/zeroconf/*.h")
    install(FILES ${OSCPACK3} DESTINATION include/madronalib/oscpack)

else()
    #TODO OSC For win / Linux
endif()


#--------------------------------------------------------------------
# Add libraries
#--------------------------------------------------------------------

if(APPLE)
  target_link_libraries(madronalib "-framework Carbon")
  target_link_libraries(madronalib "-framework CoreServices")
  target_link_libraries(madronalib "-framework AudioToolbox")
  target_link_libraries(madronalib "-framework AudioUnit")
  target_link_libraries(madronalib "-framework CoreAudio")
  target_link_libraries(madronalib "-framework Foundation")
  target_link_libraries(madronalib "-framework OpenGL")
  target_link_libraries(madronalib "-framework GLUT")
else(APPLE)
  #target_link_libraries(madronalib ${DNSSD_LIBRARIES})
endif()

if (BUILD_SHARED_LIBS)
    if (WIN32)
        # The MADRONALIB DLL needs a special compile-time macro and import library name
        set_target_properties(madronalib PROPERTIES PREFIX "" IMPORT_PREFIX "")

        if (MINGW)
            set_target_properties(madronalib PROPERTIES IMPORT_SUFFIX "dll.a")
        else()
            set_target_properties(madronalib PROPERTIES IMPORT_SUFFIX "dll.lib")
        endif()
    elseif (APPLE)
        # Append -fno-common to the compile flags to work around a bug in
        # Apple's GCC
        get_target_property(madronalib_CFLAGS madronalib COMPILE_FLAGS)
        if (NOT madronalib_CFLAGS)
            set(madronalib_CFLAGS "")
        endif()
        set_target_properties(madronalib PROPERTIES
                              COMPILE_FLAGS "${madronalib_CFLAGS} -fno-common"
                              INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/lib${LIB_SUFFIX}")
    endif()

    target_link_libraries(madronalib ${madronalib_LIBRARIES})
endif()
