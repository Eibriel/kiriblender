
set(INC
	.
	../device
	../kernel
	../kernel/svm
	../bvh
	../util
	../render
	../subd
)
set(INC_SYS
)

set(LIBRARIES
	cycles_device
	cycles_kernel
	cycles_render
	cycles_bvh
	cycles_subd
	cycles_util
	${BOOST_LIBRARIES}
	${OPENEXR_LIBRARIES}
	${OPENGL_LIBRARIES}
	${CYCLES_GLEW_LIBRARY}
	${OPENIMAGEIO_LIBRARIES}
	${PNG_LIBRARIES}
	${JPEG_LIBRARIES}
	${ZLIB_LIBRARIES}
	${TIFF_LIBRARY}
)

if(WIN32)
	list(APPEND LIBRARIES ${PTHREADS_LIBRARIES})
endif()

link_directories(${OPENIMAGEIO_LIBPATH} ${BOOST_LIBPATH} ${PNG_LIBPATH} ${JPEG_LIBPATH} ${ZLIB_LIBPATH} ${TIFF_LIBPATH})

if(WITH_CYCLES_STANDALONE AND WITH_CYCLES_STANDALONE_GUI)
	list(APPEND LIBRARIES ${GLUT_LIBRARIES})
endif()

if(WITH_CYCLES_OSL)
	list(APPEND LIBRARIES cycles_kernel_osl ${OSL_LIBRARIES})
endif()

include_directories(${INC})
include_directories(SYSTEM ${INC_SYS})

if(WITH_CYCLES_STANDALONE)
	set(SRC
		cycles_standalone.cpp
		cycles_xml.cpp
		cycles_xml.h
	)
	add_executable(cycles ${SRC})
	target_link_libraries(cycles ${LIBRARIES} ${CMAKE_DL_LIBS})

	if(UNIX AND NOT APPLE)
		set_target_properties(cycles PROPERTIES INSTALL_RPATH $ORIGIN/lib)
	endif()
	unset(SRC)
endif()

if(WITH_CYCLES_NETWORK)
	set(SRC
		cycles_server.cpp
	)
	add_executable(cycles_server ${SRC})
	target_link_libraries(cycles_server ${LIBRARIES})

	if(UNIX AND NOT APPLE)
		set_target_properties(cycles_server PROPERTIES INSTALL_RPATH $ORIGIN/lib)
	endif()
	unset(SRC)
endif()

