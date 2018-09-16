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

static void
shell_loop()
{
	int i, argv_sz;
	pid_t pid;
	char cmd_buf[ARG_MAX], **argv;

	while (true) {
		printf("> ");

		/* Read from stdin. Then fork and exec the new command. */
		fgets(cmd_buf, ARG_MAX, stdin);

		/* Parse the string and get the command line arguments to pass. */

		argv = calloc(ARGV_CHUNK_SZ, sizeof(*argv));
		if (argv == NULL) {
			err(ENOMEM, "Failed to allocate argv buffer");
		}
		argv_sz = ARGV_CHUNK_SZ;

		argv[0] = strtok(cmd_buf, " \n");
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

		pid = fork();
		if (pid < 0) {
			err(errno, "Fork failed");
		}

		if (pid == 0) {
			execvp(argv[0], argv);
			DPRINTF("execlp returned %d\n", errno);
			printf("%s: %s\n", argv[0], strerror(errno));
			exit(errno);
		} else {
			wait(NULL);
			free(argv);
		}

		/* Now read another command. */
	}
}

int
main()
{
	/* Shell init code goes here. There is nothing right now. */

	/* The main loop of the shell. Does not return. */
	shell_loop();

	/* NOTREACHED */
	ASSERT(0, "Reached not reachable section");
}
