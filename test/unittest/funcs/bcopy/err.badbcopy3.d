/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	bcopy should not copy from one memory location to another
 *	if the memory is not properly allocated
 *
 * SECTION: Actions and Subroutines/bcopy()
 *
 */

#pragma D option quiet

dtrace:::BEGIN
{
	/* Attempt to copy to a NULL address */
	bcopy((void *)&`max_pfn, (void *)NULL, sizeof(unsigned long));
	exit(0);
}

dtrace:::ERROR
{
	exit(1);
}
