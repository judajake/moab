# Things to build
lib_LTLIBRARIES = libMOAB.la 

# Subdirectories to build
SUBDIRS = 
libMOAB_la_LIBADD = 
if PARALLEL
  SUBDIRS += parallel
  libMOAB_la_LIBADD += parallel/libMOABpar.la
  AM_CPPFLAGS += -I$(srcdir)/parallel
endif
SUBDIRS += io LocalDiscretization .
libMOAB_la_LIBADD += io/libmoabio.la LocalDiscretization/libLocalDiscretization.la
if OLD_HEADERS
  SUBDIRS += oldinc
endif

BUILT_SOURCES = MBCN_protos.h

# Some variables
AM_CPPFLAGS += -DIS_BUILDING_MB \
               -I. \
               -I$(srcdir)/io \
               -I$(srcdir)/io/mhdf/include \
               -I$(srcdir)/parallel \
               -I$(srcdir)/LocalDiscretization

# The directory in which to install headers
libMOAB_la_includedir = $(includedir)

# The list of source files, and any header files that do not need to be installed
libMOAB_la_SOURCES = \
  AdaptiveKDTree.cpp \
  AEntityFactory.cpp \
  AEntityFactory.hpp \
  AffineXform.cpp \
  AffineXform.hpp \
  AxisBox.cpp \
  AxisBox.hpp \
  BitPage.cpp \
  BitPage.hpp \
  BitTag.cpp \
  BitTag.hpp \
  BoundBox.cpp \
  BSPTree.cpp \
  BSPTreePoly.cpp \
  BVHTree.cpp \
  CN.cpp \
  CartVect.cpp \
  Core.cpp \
  DebugOutput.cpp \
  DebugOutput.hpp \
  DenseTag.hpp \
  DenseTag.cpp \
  DualTool.cpp \
  ElementSequence.hpp \
  EntitySequence.cpp \
  EntitySequence.hpp \
  Factory.cpp \
  FBEngine.cpp \
  FileOptions.cpp \
  GeomUtil.cpp \
  GeomTopoTool.cpp \
  HigherOrderFactory.cpp \
  HomXform.cpp \
  Internals.hpp \
  MBCNArrays.hpp \
  MergeMesh.cpp \
  MeshSet.cpp \
  MeshSet.hpp \
  MeshSetSequence.cpp \
  MeshSetSequence.hpp \
  MeshTag.hpp \
  MeshTag.cpp \
  MeshTopoUtil.cpp \
  OrientedBox.cpp \
  OrientedBox.hpp \
  OrientedBoxTreeTool.cpp \
  lotte/poly.c \
  lotte/findpt.c \
  lotte/errmem.c \
  lotte/tensor.c \
  PolyElementSeq.cpp \
  PolyElementSeq.hpp \
  ProgOptions.cpp \
  Range.cpp \
  RangeSeqIntersectIter.cpp \
  RangeSeqIntersectIter.hpp \
  ReadUtil.cpp \
  ReadUtil.hpp \
  ReaderWriterSet.cpp \
  ReorderTool.cpp \
  ScdElementData.cpp \
  ScdElementData.hpp \
  ScdInterface.cpp \
  ScdVertexData.cpp \
  ScdVertexData.hpp \
  SequenceData.hpp \
  SequenceData.cpp \
  SequenceManager.cpp \
  SequenceManager.hpp \
  SetIterator.cpp \
  Skinner.cpp \
  SmoothCurve.cpp \
  SmoothCurve.hpp \
  SmoothFace.cpp \
  SmoothFace.hpp \
  SparseTag.cpp \
  SparseTag.hpp \
  SpatialLocator.cpp \
  SpectralMeshTool.cpp \
  StructuredElementSeq.cpp \
  StructuredElementSeq.hpp \
  SweptElementData.cpp \
  SweptElementData.hpp \
  SweptElementSeq.cpp \
  SweptElementSeq.hpp \
  SweptVertexData.cpp \
  SweptVertexData.hpp \
  SysUtil.cpp \
  SysUtil.hpp \
  TagCompare.hpp \
  TagInfo.cpp \
  TagInfo.hpp \
  Tree.cpp \
  Types.cpp \
  TypeSequenceManager.cpp \
  TypeSequenceManager.hpp \
  UnstructuredElemSeq.cpp \
  UnstructuredElemSeq.hpp \
  Util.cpp \
  VarLenDenseTag.hpp \
  VarLenDenseTag.cpp \
  VarLenSparseTag.hpp \
  VarLenSparseTag.cpp \
  VarLenTag.hpp \
  VertexSequence.hpp \
  VertexSequence.cpp \
  WriteUtil.cpp \
  WriteUtil.hpp \
  moab_mpe.h

# The list of header files which are to be installed
nobase_libMOAB_la_include_HEADERS = \
  moab/AdaptiveKDTree.hpp \
  moab/BoundBox.hpp \
  moab/BSPTree.hpp \
  moab/BSPTreePoly.hpp \
  moab/BVHTree.hpp \
  moab/CN.hpp \
  moab/CartVect.hpp \
  moab/Compiler.hpp \
  moab/Core.hpp \
  moab/CpuTimer.hpp \
  moab/DualTool.hpp \
  moab/Error.hpp \
  moab/GeomTopoTool.hpp \
  moab/HigherOrderFactory.hpp \
  moab/HomXform.hpp \
  moab/EntityType.hpp \
  moab/EntityHandle.hpp \
  moab/FBEngine.hpp \
  moab/FileOptions.hpp \
  moab/FindPtFuncs.h \
  moab/Forward.hpp \
  moab/GeomUtil.hpp \
  moab/Interface.hpp \
  moab/point_locater/tree/common_tree.hpp \
  moab/point_locater/tree/element_tree.hpp \
  moab/point_locater/tree/bvh_tree.hpp \
  moab/point_locater/io.hpp  \
  moab/point_locater/element_maps/linear_hex_map.hpp  \
  moab/point_locater/element_maps/linear_tet_map.hpp  \
  moab/point_locater/element_maps/spectral_hex_map.hpp  \
  moab/point_locater/element_maps/quadratic_hex_map.hpp  \
  moab/point_locater/point_locater.hpp \
  moab/point_locater/parametrizer.hpp \
  moab/Matrix3.hpp \
  moab/MergeMesh.hpp \
  moab/MeshTopoUtil.hpp \
  moab/OrientedBoxTreeTool.hpp \
  moab/ProgOptions.hpp \
  moab/Range.hpp \
  moab/RangeMap.hpp \
  moab/ReadUtilIface.hpp \
  moab/ReaderIface.hpp \
  moab/ReaderWriterSet.hpp \
  moab/ReorderTool.hpp \
  moab/ScdInterface.hpp \
  moab/SetIterator.hpp \
  moab/Skinner.hpp \
  moab/SpatialLocator.hpp \
  moab/SpectralMeshTool.hpp \
  moab/Tree.hpp \
  moab/TreeStats.hpp \
  moab/Types.hpp \
  moab/UnknownInterface.hpp \
  moab/Util.hpp \
  moab/WriteUtilIface.hpp \
  moab/WriterIface.hpp \
  MBEntityType.h \
  MBCN.h \
  MOAB_FCDefs.h \
  MBCN_protos.h \
  MBTagConventions.hpp

if ENABLE_mbzoltan
  libMOAB_la_SOURCES += $(srcdir)/../tools/mbzoltan/MBZoltan.cpp
  nobase_libMOAB_la_include_HEADERS += $(srcdir)/../tools/mbzoltan/MBZoltan.hpp
  AM_CPPFLAGS += $(ZOLTAN_INC_FLAGS)
  # not sure yet if this is needed; will see when linking io reader (MPAS so far)
  # the tool mbpart needed it, and it just includes MBZoltan.hpp; so it needs ways to find 
  # the libraries zoltan depends on
  libMOAB_la_LIBADD += $(ZOLTAN_LIBS) $(ZOLTAN_LIB_FLAGS)
endif

if HAVE_CGM
  AM_CPPFLAGS += @CGM_CPPFLAGS@ -DCGM @MOAB_CGM_DEFINES@
endif

MBCN_protos.h: MBCN.h $(top_srcdir)/itaps/mkprotos.sh
	$(AM_V_GEN)$(top_srcdir)/itaps/mkprotos.sh MBCN MOAB $< $@ MOAB_FCDefs.h

MOAB_FCDefs.h: FCDefs.h
	$(AM_V_GEN)cd .. && ./config.status src/MOAB_FCDefs.h

DISTCLEANFILES = MOAB_FCDefs.h
