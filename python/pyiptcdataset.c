#include <Python.h>

#if PY_MAJOR_VERSION >= 3
# include "python3/pyiptcdataset.c"
#else
# include "python2/pyiptcdataset.c"
#endif
