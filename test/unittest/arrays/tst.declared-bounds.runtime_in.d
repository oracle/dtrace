/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Within-bounds array assignments work even when index
 *	      requires run-time check.
 *
 * SECTION: Pointers and Arrays/Array Declarations and Storage
 */

#pragma D option quiet

int a[5];

dtrace:::BEGIN
{
	i = 0;
	a[i] = 12345678;
	trace(a[i]);
	exit(0);
}

dtrace:::ERROR
{
	trace("run-time error");
	exit(1);
}
