AM_CPPFLAGS = -iquote $(top_srcdir) -iquote $(top_srcdir)/cwds

bin_PROGRAMS = montecarlo

montecarlo_SOURCES = \
	montecarlo.cxx

montecarlo_CXXFLAGS = -std=c++11 @LIBCWD_R_FLAGS@
montecarlo_LDADD = ../statefultask/libstatefultask.la ../threadsafe/libthreadsafe.la ../utils/libutils_r.la ../cwds/libcwds_r.la

# --------------- Maintainer's Section

MAINTAINERCLEANFILES = $(srcdir)/Makefile.in
