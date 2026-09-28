#include "gl.h"
#define VRGL_DEF(ret, name, ...) PFN_##name name = nullptr;
VRGL_FUNCS(VRGL_DEF)
#undef VRGL_DEF

bool vrglLoad(void *(*getProc)(const char *), const char **missing) {
#define VRGL_LOAD(ret, name, ...) \
    name = reinterpret_cast<PFN_##name>(getProc(#name)); \
    if (!name) { if (missing) *missing = #name; return false; }
    VRGL_FUNCS(VRGL_LOAD)
#undef VRGL_LOAD
    return true;
}
