/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	memory allocated by alloca() is only valid within the clause
 *	it is allocated.
 *
 * SECTION: Actions and Subroutines/alloca()
 *
 */

#pragma D option quiet


dtrace:::BEGIN
{
	ptr = alloca(sizeof(unsigned long));
}

profile:::tick-1
{
	bcopy((void *)&`max_pfn, ptr, sizeof(unsigned long));
	exit(0);
}

dtrace:::ERROR
{
	exit(1);
}
