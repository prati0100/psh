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

/* Number of alias string pointers to allocate at a time. */
#define ALIAS_CHUNK_SZ 20

struct psh_alias *aliases;
int num_aliases;

static void psh_cd(char **);
static void psh_exit(char **);
static void psh_add_alias(char **);

static void (*builtin_handlers[])(char **) = {
	psh_add_alias,
	psh_cd,
	psh_exit
};

static void
psh_exit(char **argv)
{
	/* The clean-up code goes here. */
	exit(0);
}

static void
psh_add_alias(char **argv)
{
	int valsz, i;
	char *valstr;
	ASSERT(strcmp(argv[0], "alias") == 0, "Not an alias command");

	/* TODO: Expand the alias array when we run out. */

	if (num_aliases == -1) {
		printf("Error! Can't add alias. No more memory available\n");
		return;
	}

	/* Make sure that alias does not already exist. */
	for (i = 0; i < num_aliases; i++) {
		if (strcmp(argv[1], aliases[i].name) == 0) {
			printf("The alias for \"%s\" already exists\n", argv[1]);
		}
	}

	if (argv[2] == NULL) {
		printf("Error! No alias value specified\n");
		return;
	}

	aliases[num_aliases].name = strdup(argv[1]);

	/* Calculate the total size of the value field of the alias. */
	for (valsz = 0, i = 2; argv[i] != NULL; i++) {
		/* strlen() + 1 because of the space that was stripped when parsing. */
		valsz += strlen(argv[i]) + 1;
	}

	valstr = malloc(sizeof(*valstr) * valsz);

	/* Construct the value string by adding spaces before each argv. */
	for (i = 2; argv[i] != NULL; i++) {
		strcat(valstr, argv[i]);
		strcat(valstr, " ");
	}

	/* Remove the last space. */
	valstr[strlen(valstr) - 1] = 0;

	aliases[num_aliases].value = valstr;
	num_aliases++;
}

static void
psh_expand_alias(char *cmd)
{
	int i, j, sz, shift;

	/* Get the size of the command. */
	for (sz = 0; cmd[sz] != ' ' && cmd[sz] != 0; sz++);

	for (i = 0; i < num_aliases; i++) {
		if (strncmp(aliases[i].name, cmd, sz) == 0) {
			shift = strlen(aliases[i].value) - sz;
			for (j = strlen(cmd); j >= sz; j--) {
				cmd[j + shift] = cmd[j];
			}

			for (j = 0; j < strlen(aliases[i].value); j++) {
				cmd[j] = aliases[i].value[j];
			}

			return;
		}
	}
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
	char cmd_buf[ARG_MAX], **argv, *cwd, *token;

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

		psh_expand_alias(cmd_buf);

		token = strtok(cmd_buf, " \n");
		if (token == NULL) {
			continue;
		}

		argv[0] = strdup(token);
		for (i = 1; (token = strtok(NULL, " \n")) != NULL; i++) {
			argv[i] = strdup(token);
			if (argv[i] == NULL) {
				err(ENOMEM, "Failed to allocate argument string");
			}

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

		for (i = 0; argv[i] != NULL; i++) {
			free(argv[i]);
		}
		free(argv);
		free(cwd);

		/* Now read another command. */
	}
}

int
main()
{
	/* Shell init code goes here. */

	/* Ignore SIGINT. */
	struct sigaction sigint_act;
	sigint_act.sa_handler = SIG_IGN;
	sigaction(SIGINT, &sigint_act, NULL);

	num_aliases = 0;
	aliases = calloc(ALIAS_CHUNK_SZ, sizeof(*aliases));
	if (aliases == NULL) {
		printf("Failed to allocate an array for aliases: %s\n",
			strerror(ENOMEM));
		num_aliases = -1;
	}

	/* The main loop of the shell. Does not return. */
	psh_loop();

	/* NOTREACHED */
	ASSERT(0, "Reached not reachable section");
}
