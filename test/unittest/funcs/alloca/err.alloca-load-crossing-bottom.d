/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Loads crossing the bottom of alloca()ed memory fail.
 *
 * SECTION: Actions and Subroutines/alloca()
 */

#pragma D option quiet

dtrace:::BEGIN
{
	s = (int *)alloca(10);
	trace(s[-1]);
}

dtrace:::ERROR
{
	exit(1);
}
