/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The printf() action does not allow precision before width.
 *
 * SECTION: Actions and Subroutines/printf()
 */

#pragma D option quiet

dtrace:::BEGIN
{
	printf("%.**d");
	exit(0);
}
