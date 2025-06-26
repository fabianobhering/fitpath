#ifndef __MEMORY_H__
#define __MEMORY_H__

#include <stdlib.h>
#include <stdio.h>

#if __x86_64__
#define MALLOC(var, size) do {\
							var = (__typeof__(var)) malloc(size);\
							if (!var) {\
								fprintf(stderr, "Out of memory trying to allocate %lu bytes at %s:%d\n", (size), __FILE__, __LINE__);\
								exit(1);\
							}\
						} while(0)

#define REALLOC(var, size) do {\
							var = (__typeof__(var)) realloc(var, size);\
							if (!var) {\
								fprintf(stderr, "Out of memory trying to allocate %lu bytes at %s:%d\n", (size), __FILE__, __LINE__);\
								exit(1);\
							}\
						} while(0)
#else
#define MALLOC(var, size) do {\
							var = (__typeof__(var)) malloc(size);\
							if (!var) {\
								fprintf(stderr, "Out of memory trying to allocate %u bytes at %s:%d\n", (size), __FILE__, __LINE__);\
								exit(1);\
							}\
						} while(0)

#define REALLOC(var, size) do {\
							var = (__typeof__(var)) realloc(var, size);\
							if (!var) {\
								fprintf(stderr, "Out of memory trying to allocate %u bytes at %s:%d\n", (size), __FILE__, __LINE__);\
								exit(1);\
							}\
						} while(0)

#endif



#endif
