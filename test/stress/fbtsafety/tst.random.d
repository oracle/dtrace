/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * 	trace rand() at every fbt probe point.
 *
 * SECTION: Actions and Subroutines/rand();
 *	Options and Tunables/bufsize;
 * 	Options and Tunables/bufpolicy
 */

/*
 * We set our buffer size absurdly low to prevent a flood of errors that we
 * don't care about.
 */

#pragma D option bufsize=16
#pragma D option bufpolicy=ring

fbt:::
{
	n++;
	trace(*(int *)rand());
}

dtrace:::ERROR
{
	err++;
}

profile:::tick-1sec
/sec++ == 10/
{
	exit(2);
}

dtrace:::END
/n == 0 || err == 0/
{
	exit(1);
}

dtrace:::END
/n != 0 && err != 0/
{
	exit(0);
}
