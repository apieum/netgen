include_HEADERS = nglib.h

dist_pkgdata_DATA = cube.surf

AM_CPPFLAGS = -I$(top_srcdir)/libsrc/include  $(MPI_INCLUDES) $(OCCFLAGS)

lib_LTLIBRARIES = libnglib.la
libnglib_la_SOURCES = nglib.cpp 

libnglib_la_LIBADD = \
	$(top_builddir)/libsrc/interface/libnginterface.la \
	$(top_builddir)/libsrc/geom2d/libnggeom2d.la \
	$(top_builddir)/libsrc/csg/libngcsg.la \
	$(top_builddir)/libsrc/stlgeom/libngstl.la \
	$(top_builddir)/libsrc/occ/libngocc.la \
	$(top_builddir)/libsrc/meshing/libngmesh.la \
	$(OCCLIBS) $(MPI_LIBS)

libnglib_la_LDFLAGS = -release $(PACKAGE_VERSION)
#  -rdynamic


bin_PROGRAMS = ng_vol ng_stl
#  ng_occ

ng_vol_SOURCES = ng_vol.cpp
ng_vol_LDADD = libnglib.la \
	$(top_builddir)/libsrc/interface/libnginterface.la \
	$(top_builddir)/libsrc/geom2d/libnggeom2d.la \
	$(top_builddir)/libsrc/csg/libngcsg.la \
	$(top_builddir)/libsrc/stlgeom/libngstl.la \
	$(top_builddir)/libsrc/occ/libngocc.la \
	$(top_builddir)/libsrc/meshing/libngmesh.la \
	$(OCCLIBS) $(MPI_LIBS)



ng_stl_SOURCES = ng_stl.cpp
ng_stl_LDADD = libnglib.la \
	$(top_builddir)/libsrc/interface/libnginterface.la \
	$(top_builddir)/libsrc/geom2d/libnggeom2d.la \
	$(top_builddir)/libsrc/csg/libngcsg.la \
	$(top_builddir)/libsrc/stlgeom/libngstl.la \
	$(top_builddir)/libsrc/occ/libngocc.la \
	$(top_builddir)/libsrc/meshing/libngmesh.la \
	$(OCCLIBS) $(MPI_LIBS)

# ng_occ_SOURCES = ng_occ.cpp
# ng_occ_LDADD = libnglib.la 


