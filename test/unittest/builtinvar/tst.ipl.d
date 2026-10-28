/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2: need ipl support */

/*
 * ASSERTION:
 * To print variable ipl
 *
 * SECTION: Variables/Built-in Variables
 */

#pragma D option quiet

dtrace:::BEGIN
{
}

profile:::tick-10ms
{
	printf("The interrupt priority level = %u\n", ipl);
	exit(0);
}

dtrace:::ERROR
{
	exit(1);
}
