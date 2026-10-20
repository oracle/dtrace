/*
 * Oracle Linux DTrace.
 * Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: strlen() must not overwrite the zeros block used to initialize
 *            aggregations in the strtab map.
 *
 * SECTION: Actions and Subroutines/strlen()
 */
/* @@nosort */

#pragma D option quiet
#pragma D option strsize=128

BEGIN
{
	/*
	 * With no IPv6 lookup table, rodata contains only its reserved byte.
	 * This string reaches past it and all padding into the zeros block.
	 */
	printf("length: %d\n", strlen(
	    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"));

	/* A new aggregation copies its initial value from the zeros block. */
	@total = sum(1);
	printa("total: %@d\n", @total);
	clear(@total);
	exit(0);
}

ERROR
{
	exit(1);
}
