#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>
#include <string.h>

#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <psh.h>

/* Amount of argv pointers to allocate at a time. */
#define ARGV_CHUNK_SZ 10

/* Number of alias string pointers to allocate at a time. */
#define ALIAS_CHUNK_SZ 20

#define PSH_RCBUF_SZ 150

struct psh_alias *aliases;
int num_aliases;

/*
 * Shell builtin command handlers.
 */

int
psh_exit(char **argv)
{
	/* The clean-up code goes here. */
	exit(0);
}

int
psh_add_alias(char **argv)
{
	int valsz, i;
	char *valstr;
	ASSERT(strcmp(argv[0], "alias") == 0, "Not an alias command");

	/* TODO: Expand the alias array when we run out. */

	if (num_aliases == -1) {
		printf("Error! Can't add alias. No more memory available\n");
		return ENOMEM;
	}

	/* Make sure that alias does not already exist. */
	for (i = 0; i < num_aliases; i++) {
		if (strcmp(argv[1], aliases[i].name) == 0) {
			printf("The alias for \"%s\" already exists\n", argv[1]);
			return EINVAL;
		}
	}

	if (argv[2] == NULL) {
		printf("Error! No alias value specified\n");
		return EINVAL;
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

	return 0;
}

int
psh_cd(char **argv)
{
	if (chdir(argv[1])) {
		printf("Error! %s\n", strerror(errno));
		return errno;
	}

	return 0;
}

void
psh_expand_alias(char *cmd)
{
	int i, j, sz, shift;

	/* Get the size of the command. */
	for (sz = 0; cmd[sz] != ' ' && cmd[sz] != 0; sz++);

	if (sz == 0) {
		return;
	}

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

/*
 * Check if the command list passed is a shell builtin command. If it is,
 * execute it and return true. Otherwise, return false. If RETVAL is not NULL,
 * fill it with the return value of the builtin handler.
 */
static bool
psh_check_builtin(char **argv, int *retval)
{
	int i, error;

	for (i = 0; i < PSH_NUM_BUILTINS; i++) {
		if (strcmp(argv[0], psh_builtins[i].name) == 0) {
			error = psh_builtins[i].handler(argv);
			if (retval != NULL) {
				*retval = error;
			}
			return true;
		}
	}

	return false;
}

static int
psh_exec(char *cmd)
{
	char **argv, *token;
	pid_t pid;
	int i, argv_sz, error;

	argv = calloc(ARGV_CHUNK_SZ, sizeof(*argv));
	if (argv == NULL) {
		printf("Failed to allocate argv buffer: %s", strerror(ENOMEM));
		return ENOMEM;
	}
	argv_sz = ARGV_CHUNK_SZ;

	psh_expand_alias(cmd);

	token = strtok(cmd, " \n");
	if (token == NULL) {
		return 0;
	}

	argv[0] = strdup(token);
	for (i = 1; (token = strtok(NULL, " \n")) != NULL; i++) {
		argv[i] = strdup(token);
		if (argv[i] == NULL) {
			printf("Failed to allocate argument string: %s", strerror(ENOMEM));
			return ENOMEM;
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

	/* Check if the command is a shell-builtin. If yes, handle it separately. */
	if (psh_check_builtin(argv, &error)) {
		return error;
	}

	pid = fork();
	if (pid < 0) {
		printf("Fork failed: %s", strerror(errno));
		return errno;
	}

	if (pid == 0) {
		execvp(argv[0], argv);
		DPRINTF("execlp returned %d\n", errno);
		printf("%s: %s\n", argv[0], strerror(errno));
		exit(errno);
	}

	wait(NULL);

	for (i = 0; argv[i] != NULL; i++) {
		free(argv[i]);
	}
	free(argv);

	return 0;
}

static void
psh_loop()
{
	int i, ch;
	char cmd_buf[ARG_MAX], *cwd;

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

		psh_exec(cmd_buf);

		free(cwd);
		/* Now read another command. */
	}
}

/* Read and execute the .pshrc file. */
static int
psh_readrc()
{
	FILE *rcfile;
	char *rcpath, *home, *buf;
	int error;

	error = 0;

	home = getenv("HOME");

	/* We need to allocate memory for $HOME/.pshrc */
	rcpath = malloc(sizeof(*rcpath) * (strlen(home) + sizeof(PSHRC_PATH) + 1));
	if (rcpath == NULL) {
		error = ENOMEM;
		goto err;
	}

	/* strcat needs a nul-terminator to work. */
	rcpath[0] = 0;
	strcat(rcpath, home);
	strcat(rcpath, "/");
	strcat(rcpath, PSHRC_PATH);

	rcfile = fopen(rcpath, "r");
	if (rcfile == NULL) {
		error = errno;
		goto err_rcpath;
	}

	buf = malloc(sizeof(*buf) * PSH_RCBUF_SZ);
	if (buf == NULL) {
		error = ENOMEM;
		goto err_rcpath;
	}

	while (fgets(buf, PSH_RCBUF_SZ, rcfile) != NULL) {
		if (buf[strlen(buf) - 1] != '\n') {
			printf("Line length too large\n");
			continue;
		}

		/* Remove the trailing newline. */
		buf[strlen(buf) - 1] = 0;

		DPRINTF("%s\n", buf);
		psh_exec(buf);
	}

	fclose(rcfile);

	free(buf);
	free(rcpath);
	return 0;

err_rcpath:
	free(rcpath);
err:
	ASSERT(error != 0, "On error handling path, but errno == 0");
	DPRINTF("Failed to open %s: %s\n", PSHRC_PATH, strerror(error));
	return error;
}

int
main()
{
	/* Shell initialization. */

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

	psh_readrc();

	/* The main loop of the shell. Does not return. */
	psh_loop();

	/* NOTREACHED */
	ASSERT(0, "Reached not reachable section");
}
