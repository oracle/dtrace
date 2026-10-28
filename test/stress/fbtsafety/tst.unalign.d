/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * We set our buffer size absurdly low to prevent a flood of errors that we
 * don't care about.  Note that this test (unlike some other safety tests)
 * doesn't check the error count -- some architectures have no notion of
 * alignment.
 */
#pragma D option bufsize=16
#pragma D option bufpolicy=ring

fbt:::
{
	n++;
	trace(*(int *)(rand() | 1));
}

profile:::tick-1sec
/sec++ == 10/
{
	exit(2);
}

dtrace:::END
/n == 0/
{
	exit(1);
}

dtrace:::END
/n != 0/
{
	exit(0);
}
