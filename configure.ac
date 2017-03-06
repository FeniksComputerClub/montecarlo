# Define package version here.
#
# CW_VERSION_MAJOR               : Increment when the whole library was rewritten; or some VERY MAJOR change was made.
# CW_VERSION_MINOR               : Increment when major changes have occured, like removing interfaces.
# CW_VERSION_REVISION            : Increment every public release; or set to 0 when CW_VERSION_MINOR was incremented.

define(CW_VERSION_MAJOR, 0)
define(CW_VERSION_MINOR, 0)
define(CW_VERSION_REVISION, 0)

# Package name and bug reporting.
define(CW_PACKAGE_NAME, [montecarlo])
define(CW_BUGREPORT, [])

# Version info, see http://www.gnu.org/software/libtool/manual.html#Updating-version-info
#
# CW_INTERFACE_VERSION           : Current interface version, increment whenever the interface is changed.
# CW_INTERFACE_VERSION_REVISION  : Revision, increment every public release; or set to 0 when the interface is changed.
# CW_INTERFACE_AGE               : Increment if interfaces have been added; or set to 0 when interfaces have been removed.

# CW_COMPILE_FLAGS may contain warning flags, but also default debug flags (-g*)
# and optimization flags (-O*) that which will be stripped when not required.
define(CW_COMPILE_FLAGS, [-fmax-errors=2 -W -Wall -Woverloaded-virtual -Wundef -Wpointer-arith -Wwrite-strings -Winline])
# CW_THREADS can be [no] (single-threaded), [yes] (multi-threaded) or [both] (single and multi-threaded applications).
define(CW_THREADS, [yes])

# Include cwm4 header.
m4_include([cwm4/configure_ac_top.m4])

### Uncomment this if the project is using doxygen to generate documentation.
# We use doxygen for documentation.
#CW_DOXYGEN

# undefined macro: AX_BOOST_BASE means you need to install the package 'autoconf-archive'.
AX_BOOST_BASE([1.54.0])
# configure: error: Could not find a version of the library! Means your system is broken.
# Trying passing --with-boost-libdir=/usr/lib/x86_64-linux-gnu to configure, where the
# path contains libboost_system.so (ie, /usr/lib/i386-linux-gnu on 32bit machines).
AX_BOOST_SYSTEM
AX_BOOST_FILESYSTEM

### Uncomment this to add more configuration output files.
AC_CONFIG_FILES([src/Makefile])

# Include cwm4 footer.
m4_include([cwm4/configure_ac_bottom.m4])
