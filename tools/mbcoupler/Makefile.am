MESHDIR = $(top_srcdir)/MeshFiles/unittest

AM_CPPFLAGS += -DIS_BUILDING_MB \
               -I$(top_builddir)/src \
               -I$(top_srcdir)/src 

AM_CPPFLAGS += -DSRCDIR=$(srcdir) \
               -DMESHDIR=$(MESHDIR) \
               -I$(top_srcdir)/src \
               -I$(top_builddir)/src \
               -I$(top_srcdir)/src/parallel \
               -I$(top_srcdir)/src/moab/point_locater/lotte \
               -I$(top_srcdir)/test \
               -I$(top_srcdir)/itaps \
               -I$(top_srcdir)/itaps/imesh \
               -I$(top_builddir)/itaps \
               -I$(top_builddir)/itaps/imesh 

lib_LTLIBRARIES = libmbcoupler.la
libmbcoupler_la_LIBADD = $(top_builddir)/src/libMOAB.la $(top_builddir)/itaps/imesh/libiMesh.la

LDADD = libmbcoupler.la $(top_builddir)/src/libMOAB.la $(top_builddir)/itaps/imesh/libiMesh.la

libmbcoupler_la_SOURCES = \
   Coupler.cpp \
   ElemUtil.cpp

libmbcoupler_la_include_HEADERS = \
   Coupler.hpp \
   ElemUtil.hpp

libmbcoupler_la_includedir = $(includedir)

# Automake doesn't seem to have a directory defined for
# platform-dependent data (or include) files. So put 
# in $(libdir).  Define a $(cfgdir) to get around automake's
# check that only libraries are going in $(libdir)
cfgdir = $(libdir)

TESTS = elem_util_test element_test
check_PROGRAMS = $(TESTS) 
elem_util_test_SOURCES = ElemUtilTest.cpp
element_test_SOURCES = ElementTest.cpp
addfield_SOURCES = addfield.cpp

noinst_PROGRAMS = addfield
if PARALLEL
  TESTS += mbcoupler_test 
  check_PROGRAMS += mbcoupler_test
  noinst_PROGRAMS += ssn_test
  mbcoupler_test_SOURCES = mbcoupler_test.cpp
  ssn_test_SOURCES = ssn_test.cpp
  AM_CPPFLAGS += -I$(srcdir)/parallel
endif

MOSTLYCLEANFILES = dum.h5m
