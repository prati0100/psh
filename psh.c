#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>
#include <string.h>

#include <sys/wait.h>
#include <sys/types.h>

#include <psh.h>

/* Amount of argv pointers to allocate at a time. */
#define ARGV_CHUNK_SZ 10

static void psh_cd(char **);
static void psh_exit(char **);

static void (*builtin_handlers[])(char **) = {
	psh_cd,
	psh_exit
};

static void
psh_exit(char **argv)
{
	/* The clean-up code goes here. */
	exit(0);
}

static char *
psh_setup_cwd()
{
	int i, homelen, cwdlen;
	char *cwd, *home;

	/*
	 * TODO: cwd is allocated and freed every loop, when in most cases it
	 * doesn't even change. Try to optimize it.
	 */
	cwd = getcwd(NULL, 0);

	/* Check if this belongs to a subdirectory of HOME. */
	home = getenv("HOME");
	ASSERT(home != NULL, "HOME is NULL");
	if (strncmp(cwd, home, strlen(home)) == 0) {
		cwd[0] = HOME_SYMBOL;
		homelen = strlen(home);
		cwdlen = strlen(cwd);
		for (i = homelen; i <= cwdlen; i++) {
			cwd[i - homelen + 1] = cwd[i];
		}
	}

	return cwd;
}

static void
psh_cd(char **argv)
{
	if (chdir(argv[1])) {
		printf("Error! %s\n", strerror(errno));
	}
}

/*
 * Check if the command list passed is a shell builtin command. If it is,
 * execute it and return true. Otherwise, return false.
 */
static bool
psh_check_builtin(char **argv)
{
	int i;

	for (i = 0; psh_builtins[i] != NULL; i++) {
		if (strcmp(argv[0], psh_builtins[i]) == 0) {
			builtin_handlers[i](argv);
			return true;
		}
	}

	return false;
}

static void
psh_exec(char **argv)
{
	pid_t pid;
	/* Check if the command is a shell-builtin. If yes, handle it separately. */
	if (psh_check_builtin(argv)) {
		return;
	}

	pid = fork();
	if (pid < 0) {
		err(errno, "Fork failed");
	}

	if (pid == 0) {
		execvp(argv[0], argv);
		DPRINTF("execlp returned %d\n", errno);
		printf("%s: %s\n", argv[0], strerror(errno));
		exit(errno);
	}

	wait(NULL);
}

static void
psh_loop()
{
	int i, argv_sz, ch;
	char cmd_buf[ARG_MAX], **argv, *cwd;

	while (true) {
		cwd = psh_setup_cwd();
		printf("%s> ", cwd);

		/* Read from stdin. Then fork and exec the new command. */
		for (i = 0; (ch = fgetc(stdin)) != '\n'; i++) {
			if (ch == EOF) {
				printf("exit\n");
				psh_exit(NULL);
			}

			cmd_buf[i] = ch;
		}
		cmd_buf[i] = 0;

		/* Parse the string and get the command line arguments to pass. */

		argv = calloc(ARGV_CHUNK_SZ, sizeof(*argv));
		if (argv == NULL) {
			err(ENOMEM, "Failed to allocate argv buffer");
		}
		argv_sz = ARGV_CHUNK_SZ;

		argv[0] = strtok(cmd_buf, " \n");
		if (argv[0] == NULL) {
			continue;
		}
		for (i = 1; (argv[i] = strtok(NULL, " \n")) != NULL; i++) {
			/*
			 * Ran out of argv pointers. Re-alloc the memory and initialize
			 * the new memory to 0
			 */
			if (i == argv_sz - 1) {
				DPRINTF("Ran out of argv pointers. Re-allocing\n");
				argv_sz += ARGV_CHUNK_SZ;
				argv = realloc(argv, sizeof(*argv) * argv_sz);
				memset(argv + i + 1, 0, ARGV_CHUNK_SZ);
			}
		}

		psh_exec(argv);

		free(argv);
		free(cwd);

		/* Now read another command. */
	}
}

int
main()
{
	/* Shell init code goes here. There is nothing right now. */

	/* The main loop of the shell. Does not return. */
	psh_loop();

	/* NOTREACHED */
	ASSERT(0, "Reached not reachable section");
}
