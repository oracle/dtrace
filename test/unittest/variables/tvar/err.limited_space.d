/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 - no drops yet */

/*
 * ASSERTION: Trying to store more TLS variables than we have space for will
 *	      trigger an error.
 *
 * SECTION: Variables/Thread-Local Variables
 */

#pragma D option dynvarsize=15

dtrace:::BEGIN
{
	self->a = 1;
	self->b = 2;
	self->c = 3;
	self->d = 4;
}

dtrace:::BEGIN
{
	exit(0);
}

dtrace:::ERROR
{
	exit(1);
}
