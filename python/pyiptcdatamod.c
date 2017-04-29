#include <Python.h>

#if PY_MAJOR_VERSION >= 3
# include "python3/pyiptcdatamod.c"
#else
# include "python2/pyiptcdatamod.c"
#endif
