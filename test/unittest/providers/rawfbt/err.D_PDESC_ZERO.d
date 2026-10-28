/*
 * Oracle Linux DTrace.
 * Copyright (c) 2024, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Ensure that probing a non-existent function with the rawfbt
 *	      provider results in an error.
 */

#pragma D option quiet

dtrace:::BEGIN
{
	self->traceme = 1;
}

void bar();

rawfbt::bar:entry
{
	printf("Entering the function\n");
}

rawfbt::bar:return
{
	printf("Returning the function\n");
	exit(0);
}

