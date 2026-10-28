/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Ensure the END probe still fires after an ERROR was reported.
 *
 * SECTION: dtrace Provider
 */

#pragma D option quiet

dtrace:::ERROR
{
	printf("Error fired\n");
	exit(0);
}

dtrace:::END
{
	printf("End fired after exit\n");
}

dtrace:::BEGIN
{
	*(char *)NULL;
	exit(1);
}
