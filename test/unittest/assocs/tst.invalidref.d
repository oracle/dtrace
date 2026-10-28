/*
 * Oracle Linux DTrace.
 * Copyright (c) 2007, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 no support for index of assoc array; last_cmds[1][3]=0 dumps BPF */

/*
 * Test to ensure that invalid stores to a global associative array
 * are caught correctly.
 */

#pragma D option quiet

int last_cmds[int][4];

dtrace:::BEGIN
{
	errors = 0;
	forward = 0;
	backward = 0;
}

profile:::tick-1s
/!forward/
{
	forward = 1;
	last_cmds[1][4] = 0xdeadbeef;
}

profile:::tick-1s
/!backward/
{
	backward = 1;
	last_cmds[1][-5] = 0xdeadbeef;
}

profile:::tick-1s
/errors > 1/
{
	exit(0);
}

profile:::tick-1s
/n++ > 5/
{
	exit(1);
}

dtrace:::ERROR
/arg4 == DTRACEFLT_BADADDR/
{
	errors++;
}
