/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The printf action supports '%a' for kernel addresses.
 *
 * SECTION: Actions/printf()
 */

#pragma D option quiet

dtrace:::BEGIN
{
	printf("%a", &`max_pfn);
	exit(0);
}
