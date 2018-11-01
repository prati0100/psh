#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>
#include <string.h>

#include <termios.h>
#include <ncurses.h>

#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <psh.h>

/* Amount of argv pointers to allocate at a time. */
#define ARGV_CHUNK_SZ 10

/* Number of alias string pointers to allocate at a time. */
#define ALIAS_CHUNK_SZ 20

#define PSH_RCBUF_SZ 150

#define printf(fmt, args...) do {	\
	printw(fmt, ##args);			\
	refresh();						\
} while (0)

struct psh_alias *aliases;
int num_aliases;
char *cwd = NULL;	/* The current working directory. */

int
psh_setup_cwd()
{
	int i, homelen, cwdlen;
	char *newcwd, *home;

	newcwd = getcwd(NULL, 0);
	if (newcwd == NULL) {
		return errno;
	}

	/* Check if this belongs to a subdirectory of HOME. */
	home = getenv("HOME");
	ASSERT(home != NULL, "HOME is NULL");
	if (strncmp(newcwd, home, strlen(home)) == 0) {
		newcwd[0] = HOME_SYMBOL;
		homelen = strlen(home);
		cwdlen = strlen(newcwd);
		for (i = homelen; i <= cwdlen; i++) {
			newcwd[i - homelen + 1] = newcwd[i];
		}
	}

	free(cwd);
	cwd = strdup(newcwd);
	free(newcwd);

	return 0;
}

/*
 * Shell builtin command handlers.
 */

int
psh_exit(char **argv)
{
	/* The clean-up code goes here. */

	/* Clean up ncurses stuff. */
	endwin();

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

	psh_setup_cwd();

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
		free(argv);
		return 0;
	}

	argv[0] = strdup(token);
	for (i = 1; (token = strtok(NULL, " \n")) != NULL; i++) {
		argv[i] = strdup(token);
		if (argv[i] == NULL) {
			printf("Failed to allocate argument string: %s", strerror(ENOMEM));
			error = ENOMEM;
			goto out;
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
		goto out;
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

	/* TODO: Handle signals. */
	wait(NULL);

	error = 0;

out:
	i--;
	for (; i >= 0; i--) {
		free(argv[i]);
	}
	free(argv);

	return error;
}

static void
psh_loop()
{
	int i, ch, y, x;
	char cmd_buf[ARG_MAX];

	while (true) {
		printf("%s> ", cwd);

		/* Read from stdin. Then fork and exec the new command. */
		for (i = 0; (ch = getch()) != '\n'; i++) {
			switch (ch) {
			/* Exit the shell. */
			case CTRL('D'):
				printf("exit\n");
				psh_exit(NULL);
			/* Cancel the current input and redraw the prompt. */
			case CTRL('C'):
				printf("%s\n", keyname(ch));
				goto out;
			/* Clear the screen. Also clears the currently input text. */
			case CTRL('L'):
				clear();
				goto out;
			/* Clear the currently typed text. */
			case CTRL('U'):
				getyx(stdscr, y, x);
				for (; i > 0; i--) {
					move(y, --x);
					delch();
				}
				i = -1;
				break;
			/* Delete the previous character. */
			case KEY_BACKSPACE:
				/* There is nothing to delete. */
				if (i == 0) {
					i--;
					break;
				}
				getyx(stdscr, y, x);
				move(y, x-1);
				delch();
				/*
				 * One decrement for the backspace and one decrement for the
				 * actual char we will delete.
				 */
				i -= 2;
				break;
			default:
				/*
				 * We are in no echo mode so we have to print characters that
				 * should be echoed manually.
				 */
				printf("%c", ch);
				cmd_buf[i] = ch;
				break;
			}
		}
		printf("%c", ch);
		cmd_buf[i] = 0;
		psh_exec(cmd_buf);

out:
		/*
		 * NOTE: The semi-colon after out is needed because the compiler does not
		 * allow empty labels. The semi-colon acts as an empty statement. If you
		 * ever want to add a statement after it, remove the semi-colon.
		 */
		;
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
	struct termios old_settings, new_settings;

	/* Shell initialization. */

	/* Initialize ncurses. */
	initscr();
	/*
	 * Disable line buffering. This also delivers control characters directly
	 * without generating signals.
	 */
	raw();
	noecho();
	keypad(stdscr, TRUE);

	/*
	 * nucrses sets up the terminal in such a way that newline and carriage
	 * return are different. This messes up the output of the programs that we
	 * run because after the newline the cursor does not reset to 0. The ONLCR
	 * option expands newlines to newlines + carriage return.
	 */
	tcgetattr(0, &old_settings);
	new_settings = old_settings;
	new_settings.c_oflag = new_settings.c_oflag | ONLCR;
	tcsetattr(0, TCSANOW, &new_settings);

	num_aliases = 0;
	aliases = calloc(ALIAS_CHUNK_SZ, sizeof(*aliases));
	if (aliases == NULL) {
		printf("Failed to allocate an array for aliases: %s\n",
			strerror(ENOMEM));
		num_aliases = -1;
	}

	psh_readrc();
	if (psh_setup_cwd()) {
		DPRINTF("Failed to set up the cwd\n");
		cwd = strdup("");
	}

	/* The main loop of the shell. Does not return. */
	psh_loop();

	/* NOTREACHED */
	ASSERT(0, "Reached not reachable section");
}
