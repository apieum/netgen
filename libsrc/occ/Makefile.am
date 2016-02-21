noinst_HEADERS = occgeom.hpp occmeshsurf.hpp \
Partition_Inter2d.hxx Partition_Loop2d.hxx Partition_Loop.hxx \
Partition_Inter3d.hxx Partition_Loop3d.hxx Partition_Spliter.hxx \
Partition_Inter2d.ixx Partition_Loop2d.ixx Partition_Loop.ixx \
Partition_Inter3d.ixx Partition_Loop3d.ixx Partition_Spliter.ixx \
Partition_Inter2d.jxx Partition_Loop2d.jxx Partition_Loop.jxx \
Partition_Inter3d.jxx Partition_Loop3d.jxx Partition_Spliter.jxx \
utilities.h vsocc.hpp


AM_CPPFLAGS = -I$(top_srcdir)/libsrc/include  $(OCCFLAGS) $(TCL_INCLUDES)

# $(OCC_INC_FLAG)

METASOURCES = AUTO

lib_LTLIBRARIES = libngocc.la

if NGGUI
lib_LTLIBRARIES += libngoccvis.la 
endif


libngocc_la_SOURCES = Partition_Inter2d.cxx Partition_Inter3d.cxx \
	Partition_Loop.cxx Partition_Loop2d.cxx Partition_Loop3d.cxx Partition_Spliter.cxx \
	occconstruction.cpp occgenmesh.cpp occgeom.cpp occmeshsurf.cpp

libngocc_la_LIBADD = $(OCCLIBS)

libngoccvis_la_SOURCES = occpkg.cpp vsocc.cpp
libngoccvis_la_LIBADD = libngocc.la $(TCL_LIB_SPEC)


