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

/* The .pshrc file is stored in ~/.pshrc */
#define PSHRC_PATH ".pshrc"

#define HOME_SYMBOL '~'

struct psh_alias {
	char *name;
	char *value;
};

/* An array of current aliases. */
extern struct psh_alias *aliases;
extern int num_aliases;

/*
 * Declarations related to shell builtins.
 *
 * A shell builtin command is a command that can not be executed in a forked
 * shell instance. They need to be executed in the current shell instance. For
 * this reason, binaries of these commands can't exist.
 *
 * For example, cd is a shell builtin command. If cd was a binary in the path,
 * and someone cd'ed to a directory, the shell would fork(), and then change
 * the working directory of the child. It won't change the current working
 * directory of the shell from which the command was executed. So, cd has to be
 * implemented by the shell.
 */

/* Prototype of a built-in command handler. */
typedef void builtin_handler_t(char **args);

/* Forward declarations of the builtin functions. */
/* TODO: Try to find a way to avoid these forward declarations. */
builtin_handler_t psh_cd;
builtin_handler_t psh_exit;
builtin_handler_t psh_add_alias;

/* The built-in commands of the shell. */
struct psh_builtin {
	char *name;
	builtin_handler_t *handler;
};

struct psh_builtin psh_builtins[] = {
	{"alias", psh_add_alias},
	{"cd", psh_cd},
	{"exit", psh_exit}
};

/* TODO: Feels hackish... Maybe there is a better way. */
#define PSH_NUM_BUILTINS (sizeof(psh_builtins) / sizeof(struct psh_builtin))

#endif	/* __PSH_H */
