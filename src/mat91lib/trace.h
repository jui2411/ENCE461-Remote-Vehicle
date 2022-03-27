#ifndef TRACE_H
#define TRACE_H

#include "config.h"


#ifndef TRACE_INIT
#define TRACE_INIT

#ifdef __cplusplus
extern "C" {
#endif
    
#endif

#ifndef TRACE_PRINTF
#define TRACE_PRINTF(...) fprintf (stderr, __VA_ARGS__)
#endif

#define TRACE_INFO(THING, ...) TRACE_ ## THING ## _INFO (__VA_ARGS__)

#define TRACE_DEBUG(THING, ...) TRACE_ ## THING ## _DEBUG (__VA_ARGS__)

#define TRACE_ERROR(THING, ...) TRACE_ ## THING ## _ERROR (__VA_ARGS__)


#ifdef __cplusplus
}
#endif    
#endif


