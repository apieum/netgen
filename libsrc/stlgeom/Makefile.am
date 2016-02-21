noinst_HEADERS = meshstlsurface.hpp stlgeom.hpp stlline.hpp \
stltool.hpp stltopology.hpp vsstl.hpp

AM_CPPFLAGS = -I$(top_srcdir)/libsrc/include $(TCL_INCLUDES)
METASOURCES = AUTO

lib_LTLIBRARIES = libngstl.la 

if NGGUI
lib_LTLIBRARIES += libngstlvis.la 
endif

libngstl_la_SOURCES = meshstlsurface.cpp stlgeom.cpp stlgeomchart.cpp \
	stlgeommesh.cpp stlline.cpp stltool.cpp stltopology.cpp


libngstlvis_la_SOURCES = stlpkg.cpp vsstl.cpp
libngstlvis_la_LIBADD = libngstl.la  -lGL $(MPI_LIBS) $(TCL_LIB_SPEC)
libngstl_la_LIBADD = $(top_builddir)/libsrc/meshing/libngmesh.la $(MPI_LIBS)
# libstlvis_la_LIBADD = libstl.la $(top_builddir)/libsrc/linalg/libla.la 

