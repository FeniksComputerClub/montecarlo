AM_CPPFLAGS = -iquote $(top_srcdir) -iquote $(top_srcdir)/cwds

bin_PROGRAMS = design waittest

design_SOURCES = \
	design.cxx

design_CXXFLAGS = -std=c++11 @LIBCWD_R_FLAGS@
design_LDADD = ../statefultask/libstatefultask.la ../threadsafe/libthreadsafe.la ../utils/libutils_r.la ../cwds/libcwds_r.la @BOOST_FILESYSTEM_LIB@ @BOOST_SYSTEM_LIB@

waittest_SOURCES = \
	waittest.cxx

waittest_CXXFLAGS = -std=c++11 @LIBCWD_R_FLAGS@
waittest_LDADD = ../statefultask/libstatefultask.la ../threadsafe/libthreadsafe.la ../utils/libutils_r.la ../cwds/libcwds_r.la @BOOST_FILESYSTEM_LIB@ @BOOST_SYSTEM_LIB@

# --------------- Maintainer's Section

MAINTAINERCLEANFILES = $(srcdir)/Makefile.in
