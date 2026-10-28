/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * 	Simple array test
 *
 * SECTION: Pointers and Arrays/Array Declarations and Storage
 *
 */


#pragma D option quiet

dtrace:::BEGIN
{
	a[1] = 0;
	a[1]++;
}

profile:::tick-1
/a[1] == 1/
{
	exit(0);
}

profile:::tick-1
/a[1] != 1/
{
	printf("Expected 1, got %d\n", a[1]);
	exit(1);
}
