include_directories(${CMAKE_CURRENT_SOURCE_DIR}
                    ${CMAKE_CURRENT_BINARY_DIR})

set(SIZEOF_VOID_P ${CMAKE_SIZEOF_VOID_P})
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/iBase_f.h.in "${CMAKE_CURRENT_BINARY_DIR}/iBase_f.h" @ONLY)

set( BASE_INCLUDE_HEADERS iBase.h ${CMAKE_CURRENT_BINARY_DIR}/iBase_f.h)
option ( ENABLE_IMESH        "Should build IMESH?"       OFF )
option ( ENABLE_IGEOM        "Should build IGEOM?"       OFF )

if(ENABLE_IMESH )
  add_subdirectory(imesh)
endif()

if(ENABLE_IGEOM )
  add_subdirectory(igeom)
endif()

install(FILES ${BASE_INCLUDE_HEADERS} DESTINATION include)
