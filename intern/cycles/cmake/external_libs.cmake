###########################################################################
# Boost setup

SET(BOOST_ROOT ${CYCLES_BOOST})

SET(Boost_ADDITIONAL_VERSIONS "1.45" "1.44" 
                               "1.43" "1.43.0" "1.42" "1.42.0" 
                               "1.41" "1.41.0" "1.40" "1.40.0"
                               "1.39" "1.39.0" "1.38" "1.38.0"
                               "1.37" "1.37.0" "1.34.1" "1_34_1")
IF(LINKSTATIC)
	SET(Boost_USE_STATIC_LIBS ON)
ENDIF()

SET(Boost_USE_MULTITHREADED ON)

FIND_PACKAGE(Boost 1.34 REQUIRED COMPONENTS filesystem regex system serialization thread)

MESSAGE(STATUS "Boost found ${Boost_FOUND}")
MESSAGE(STATUS "Boost version ${Boost_VERSION}")
MESSAGE(STATUS "Boost include dirs ${Boost_INCLUDE_DIRS}")
MESSAGE(STATUS "Boost library dirs ${Boost_LIBRARY_DIRS}")
MESSAGE(STATUS "Boost libraries ${Boost_LIBRARIES}")

INCLUDE_DIRECTORIES("${Boost_INCLUDE_DIRS}")
LINK_DIRECTORIES("${Boost_LIBRARY_DIRS}")

IF(WITH_CYCLES_NETWORK)
	ADD_DEFINITIONS(-DWITH_NETWORK)
ENDIF()

IF(WITH_CYCLES_MULTI)
	ADD_DEFINITIONS(-DWITH_MULTI)
ENDIF()

###########################################################################
# OpenImageIO

FIND_LIBRARY(OPENIMAGEIO_LIBRARY NAMES OpenImageIO PATHS ${CYCLES_OIIO}/lib)
FIND_PATH(OPENIMAGEIO_INCLUDES OpenImageIO/imageio.h ${CYCLES_OIIO}/include)
FIND_PROGRAM(OPENIMAGEIO_IDIFF NAMES idiff PATHS ${CYCLES_OIIO}/bin)

IF(OPENIMAGEIO_INCLUDES AND OPENIMAGEIO_LIBRARY)
	SET(OPENIMAGEIO_FOUND TRUE)
	MESSAGE(STATUS "OpenImageIO includes = ${OPENIMAGEIO_INCLUDES}")
	MESSAGE(STATUS "OpenImageIO library = ${OPENIMAGEIO_LIBRARY}")
ELSE()
	MESSAGE(STATUS "OpenImageIO not found")
ENDIF()

ADD_DEFINITIONS(-DWITH_OIIO)
INCLUDE_DIRECTORIES(${OPENIMAGEIO_INCLUDES} ${OPENIMAGEIO_INCLUDES}/OpenImageIO)

###########################################################################
# GLUT

IF(WITH_CYCLES_TEST)
	SET(GLUT_ROOT_PATH ${CYCLES_GLUT})

	FIND_PACKAGE(GLUT)
	MESSAGE(STATUS "GLUT_FOUND=${GLUT_FOUND}")

	INCLUDE_DIRECTORIES(${GLUT_INCLUDE_DIR})
ENDIF()

###########################################################################
# OpenShadingLanguage

IF(WITH_CYCLES_OSL)

	MESSAGE(STATUS "CYCLES_OSL = ${CYCLES_OSL}")

	FIND_LIBRARY(OSL_LIBRARIES NAMES oslexec oslcomp oslquery PATHS ${CYCLES_OSL}/lib)
	FIND_PATH(OSL_INCLUDES OSL/oslclosure.h ${CYCLES_OSL}/include)
	FIND_PROGRAM(OSL_COMPILER NAMES oslc PATHS ${CYCLES_OSL}/bin)

	IF(OSL_INCLUDES AND OSL_LIBRARIES AND OSL_COMPILER)
		SET(OSL_FOUND TRUE)
		MESSAGE(STATUS "OSL includes = ${OSL_INCLUDES}")
		MESSAGE(STATUS "OSL library = ${OSL_LIBRARIES}")
		MESSAGE(STATUS "OSL compiler = ${OSL_COMPILER}")
	ELSE()
		MESSAGE(STATUS "OSL not found")
	ENDIF()

	ADD_DEFINITIONS(-DWITH_OSL)
	INCLUDE_DIRECTORIES(${OSL_INCLUDES} ${OSL_INCLUDES}/OSL ${OSL_INCLUDES}/../../../src/liboslexec)

ENDIF()

###########################################################################
# Partio

IF(WITH_CYCLES_PARTIO)

	MESSAGE(STATUS "CYCLES_PARTIO = ${CYCLES_PARTIO}")

	FIND_LIBRARY(PARTIO_LIBRARIES NAMES partio PATHS ${CYCLES_PARTIO}/lib)
	FIND_PATH(PARTIO_INCLUDES Partio.h ${CYCLES_PARTIO}/include)

	FIND_PACKAGE(ZLIB)

	IF(PARTIO_INCLUDES AND PARTIO_LIBRARIES AND ZLIB_LIBRARIES)
		LIST(APPEND PARTIO_LIBRARIES ${ZLIB_LIBRARIES})
		SET(PARTIO_FOUND TRUE)
		MESSAGE(STATUS "PARTIO includes = ${PARTIO_INCLUDES}")
		MESSAGE(STATUS "PARTIO library = ${PARTIO_LIBRARIES}")
	ELSE()
		MESSAGE(STATUS "PARTIO not found")
	ENDIF()

	ADD_DEFINITIONS(-DWITH_PARTIO)
	INCLUDE_DIRECTORIES(${PARTIO_INCLUDES})

ENDIF()

###########################################################################
# Blender

IF(WITH_CYCLES_BLENDER)
	# FIND_PATH(BLENDER_INCLUDE_DIRS RNA_blender.h PATHS ${CMAKE_BINARY_DIR}/include)
	SET(BLENDER_INCLUDE_DIRS
		${CMAKE_SOURCE_DIR}/intern/guardedalloc
		${CMAKE_SOURCE_DIR}/source/blender/makesdna
		${CMAKE_SOURCE_DIR}/source/blender/makesrna
		${CMAKE_SOURCE_DIR}/source/blender/blenloader
		${CMAKE_BINARY_DIR}/source/blender/makesrna/intern)
	IF(WIN32)
		SET(BLENDER_LIBRARIES ${CMAKE_BINARY_DIR}/bin/Release/blender.lib)
	ENDIF()
ENDIF()

###########################################################################
# CUDA

IF(WITH_CYCLES_CUDA)

	FIND_LIBRARY(CUDA_LIBRARIES NAMES cuda PATHS ${CYCLES_CUDA}/lib ${CYCLES_CUDA}/lib/Win32 NO_DEFAULT_PATH)
	FIND_PATH(CUDA_INCLUDES cuda.h ${CYCLES_CUDA}/include NO_DEFAULT_PATH)
	FIND_PROGRAM(CUDA_NVCC NAMES nvcc PATHS ${CYCLES_CUDA}/bin NO_DEFAULT_PATH)

	IF(CUDA_INCLUDES AND CUDA_LIBRARIES AND CUDA_NVCC)
		MESSAGE(STATUS "CUDA includes = ${CUDA_INCLUDES}")
		MESSAGE(STATUS "CUDA library = ${CUDA_LIBRARIES}")
		MESSAGE(STATUS "CUDA nvcc = ${CUDA_NVCC}")
	ELSE()
		MESSAGE(STATUS "CUDA not found")
	ENDIF()

	ADD_DEFINITIONS(-DWITH_CUDA)
	INCLUDE_DIRECTORIES(${CUDA_INCLUDES})

ENDIF()

###########################################################################
# OpenCL

IF(WITH_CYCLES_OPENCL)

	IF(APPLE)
		SET(OPENCL_INCLUDES "/System/Library/Frameworks/OpenCL.framework/Headers")
		SET(OPENCL_LIBRARIES "-framework OpenCL")
	ENDIF()

	IF(WIN32)
		SET(OPENCL_INCLUDES "")
		SET(OPENCL_LIRBARIES "OpenCL")
	ENDIF()

	IF(UNIX AND NOT APPLE)
		SET(OPENCL_INCLUDES ${CYCLES_OPENCL})
		SET(OPENCL_LIRBARIES "OpenCL")
	ENDIF()

	ADD_DEFINITIONS(-DWITH_OPENCL)
	INCLUDE_DIRECTORIES(${OPENCL_INCLUDES})

ENDIF()

