/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	Tests Multiple END and single BEGIN
 *
 * SECTION: dtrace Provider
 *
 */


#pragma D option quiet

dtrace:::END
{
	printf("End1 fired after exit\n");
}
dtrace:::END
{
	printf("End2 fired after exit\n");
}
dtrace:::END
{
	printf("End3 fired after exit\n");
}
dtrace:::END
{
	printf("End4 fired after exit\n");
}

dtrace:::BEGIN
{
	printf("Begin fired first\n");
	exit(0);
}
