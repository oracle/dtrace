/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2: need vtimestamp support */

/*
 * ASSERTION:
 * To print built-in variable 'vtimestamp'
 *
 * SECTION: Variables/Built-in Variables
 */

#pragma D option quiet

dtrace:::BEGIN
{
	self->t = vtimestamp;
	printf("The difftime = %d\n", vtimestamp - self->t);
	exit(0);
}

dtrace:::ERROR
{
	exit(1);
}
