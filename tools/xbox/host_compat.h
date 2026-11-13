// Host-only helper for tools/xbox/pad_test.cpp, force-included with -include.
//
// include/joy.h declares `memcpy(void*, const void*, unsigned int)`. On the 32-bit Xbox that is the same
// type as the libc one, but on a 64-bit host size_t is 64-bit and the two declarations conflict. Include
// the real <string.h> first, then rename joy.h's declaration out of the way. The test never calls it.
#include <string.h>
#define memcpy re4_host_unused_memcpy
