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
	int sz, i;
	pid_t pid;
	char cmd_buf[ARG_MAX], *progname;

	while (true) {
		printf("> ");

		/* Read from stdin. Then fork and exec the new command. */
		fgets(cmd_buf, ARG_MAX, stdin);

		/* Parse the string and get the command line arguments to pass. */

		/* Get the length of the program's name. */
		for (sz = 0; cmd_buf[sz] != ' ' && cmd_buf[sz] != '\n'; sz++);

		progname = malloc(sizeof(*progname) * sz);
		if (progname == NULL) {
			err(ENOMEM, "Failed to allocate buffer for program name");
		}

		for (i = 0; i < sz; i++) {
			progname[i] = cmd_buf[i];
		}
		/* Append the NULL terminator. */
		progname[i] = 0;

		pid = fork();
		if (pid < 0) {
			err(errno, "Fork failed");
		}

		if (pid == 0) {
			execlp(progname, progname, NULL);
			DPRINTF("execlp returned %d\n", errno);
			err(errno, "Failed to exec the program");
		} else {
			wait(NULL);
			free(progname);
			/* Now read another command. */
		}
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
