/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Test ERROR probe firing, tracing a string.
 *
 * SECTION: dtrace Provider
 */

#pragma D option quiet

dtrace:::ERROR
{
	trace("Error fired");
	exit(0);
}

dtrace:::BEGIN
{
	*(char *)NULL;
	exit(1);
}
