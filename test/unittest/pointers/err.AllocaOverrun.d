/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Demonstrating invalid memory access.
 *            (Solaris never implemented bounds checking, so accesses, even
 *            writes, beyond the alloca()ed region but within scratch space
 *            were expected to succeed.  They obviously should not.)
 *
 * SECTION: Pointers and Arrays/Pointers and Addresses
 *
 * NOTES:
 *
 */

#pragma D option quiet

dtrace:::BEGIN
{
	x = (int *)alloca(sizeof(int));
	printf("Address x: %x\n", (int)x);
	y = (int *)(x + 2);
	*y = 3;
	printf("Address y: %x\tValue: %d\n", (int)y, *y);

	exit(0);
}

dtrace:::ERROR
{
	exit(1);
}
