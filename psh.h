#ifndef __PSH_H
#define __PSH_H

#ifdef PSH_DEBUG
#define ASSERT(cond, fmt, args...)	do {									\
	if (!(cond)) {															\
		fprintf(stderr, "%s:%d -- " fmt "\n", __func__, __LINE__, ##args);	\
		exit(1);															\
	}																		\
} while (0)
#else
#define ASSERT(cond, fmt, args...)
#endif

#ifdef PSH_DEBUG
#define DPRINTF(fmt, args...) printf("%s: " fmt, __func__, ##args)
#else
#define DPRINTF(fmt, args...)
#endif

/* The maximum possible length of a shell command. */
#define ARG_MAX 1024

/* Amount of argv pointers to allocate at a time. */
#define ARGV_CHUNK_SZ 10

#endif	/* __PSH_H */
