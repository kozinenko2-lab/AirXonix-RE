// Cross-build only: the ARMHF 2.19 sysroot does not supply pthread_*_clockwait.
// A modern development host's libstdc++ headers may assume these APIs exist.
#include <bits/c++config.h>
#undef _GLIBCXX_USE_PTHREAD_COND_CLOCKWAIT
#undef _GLIBCXX_USE_PTHREAD_MUTEX_CLOCKLOCK
