noinst_HEADERS = algprim.hpp csgparser.hpp extrusion.hpp manifold.hpp \
singularref.hpp surface.hpp brick.hpp curve2d.hpp gencyl.hpp	      \
meshsurf.hpp solid.hpp triapprox.hpp csgeom.hpp edgeflw.hpp geoml.hpp \
polyhedra.hpp specpoin.hpp csg.hpp explicitcurve2d.hpp identify.hpp   \
revolution.hpp spline3d.hpp vscsg.hpp


AM_CPPFLAGS = -I$(top_srcdir)/libsrc/include  $(TCL_INCLUDES)
METASOURCES = AUTO

lib_LTLIBRARIES = libngcsg.la


libngcsg_la_SOURCES = algprim.cpp brick.cpp   \
bspline2d.cpp csgeom.cpp csgparser.cpp curve2d.cpp edgeflw.cpp	       \
explicitcurve2d.cpp extrusion.cpp gencyl.cpp genmesh.cpp identify.cpp  \
manifold.cpp meshsurf.cpp polyhedra.cpp revolution.cpp singularref.cpp \
solid.cpp specpoin.cpp spline3d.cpp surface.cpp triapprox.cpp

libngcsg_la_LIBADD =  $(top_builddir)/libsrc/meshing/libngmesh.la



if NGGUI
lib_LTLIBRARIES += libngcsgvis.la

libngcsgvis_la_SOURCES = vscsg.cpp csgpkg.cpp
libngcsgvis_la_LIBADD = libngcsg.la
endif

#   $(top_builddir)/libsrc/geom2d/libgeom2d.la 

