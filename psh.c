#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <psh.h>

static void
shell_loop()
{
	while (1) {

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
