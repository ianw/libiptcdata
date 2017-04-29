#include <Python.h>

#if PY_MAJOR_VERSION >= 3
# include "python3/pyiptcdata.c"
#else
# include "python2/pyiptcdata.c"
#endif
