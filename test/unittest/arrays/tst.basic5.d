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
	a["test"] = 0;
	b = ++a["test"];
}

profile:::tick-1
/b == 1/
{
	exit(0);
}

profile:::tick-1
/b != 1/
{
	printf("Expected b = 1, got %d\n", b);
	exit(1);
}


