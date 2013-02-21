#ifndef XMCOMP_COMMON_H
#define XMCOMP_COMMON_H

#define BOOL char
#define TRUE 1
#define FALSE 0

#define likely(x)   __builtin_expect((x),1)
#define unlikely(x) __builtin_expect((x),0)

#endif
