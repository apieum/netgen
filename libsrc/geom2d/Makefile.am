noinst_HEADERS = geom2dmesh.hpp geometry2d.hpp vsgeom2d.hpp 

AM_CPPFLAGS = -I$(top_srcdir)/libsrc/include  $(TCL_INCLUDES)

METASOURCES = AUTO

lib_LTLIBRARIES = libnggeom2d.la 

if NGGUI
lib_LTLIBRARIES += libnggeom2dvis.la 
endif



libnggeom2d_la_SOURCES = genmesh2d.cpp geom2dmesh.cpp geometry2d.cpp
libnggeom2d_la_LIBADD = $(top_builddir)/libsrc/meshing/libngmesh.la $(MPI_LIBS)

libnggeom2dvis_la_SOURCES = geom2dpkg.cpp vsgeom2d.cpp
libnggeom2dvis_la_LIBADD = libnggeom2d.la -lGL $(MPI_LIBS)


