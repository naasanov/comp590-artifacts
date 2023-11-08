if(NOT APPLE)
  set(CMAKE_FIND_DEBUG_MODE TRUE)
  find_package(OpenGL REQUIRED COMPONENTS OpenGL)
  set(CMAKE_FIND_DEBUG_MODE FALSE)
  
else()
  
  if(NOT OPENGL_FOUND)
    find_path(OPENGL_INCLUDE_DIR GL/gl.h DOC "Include for OpenGL on OS X" HINTS $ENV{CONDA_PREFIX}/x86_64-conda-linux-gnu/sysroot/usr/include )

    find_library(OPENGL_gl_LIBRARY
      NAMES GL
      HINTS $ENV{CONDA_PREFIX}/x86_64-conda-linux-gnu/sysroot/usr/lib64
    )

    find_library(OPENGL_glx_LIBRARY
      NAMES GLX
      HINTS $ENV{CONDA_PREFIX}/x86_64-conda-linux-gnu/sysroot/usr/lib64
    )
    
    if(NOT TARGET OpenGL::GL)
      add_library(OpenGL::GL INTERFACE IMPORTED)
    endif()
	
#    set_target_properties(OpenGL::GL PROPERTIES
#            INTERFACE_LINK_LIBRARIES ${OPENGL_gl_LIBRARY}
#	    INTERFACE_INCLUDE_DIRECTORIES ${OPENGL_INCLUDE_DIR}  # include path
#    )

    target_include_directories(OpenGL::GL INTERFACE ${OPENGL_INCLUDE_DIR} )
    target_link_libraries(OpenGL::GL INTERFACE
        ${OPENGL_gl_LIBRARY}
	${OPENGL_glx_LIBRARY}
    )
    
#    set_property(TARGET OpenGL::GL APPEND PROPERTY
#	    INTERFACE_LINK_LIBRARIES ${OPENGL_glx_LIBRARY}   # Library path
#    )
    
  endif()
endif()

if(OpenGL_FOUND)
  ov_print(OV_PRINTED "Found OpenGL library")
  ADD_DEFINITIONS(-DTARGET_HAS_ThirdPartyOpenGL)
endif()
