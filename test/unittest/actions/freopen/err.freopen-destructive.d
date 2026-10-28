/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: freopen() is a destructive action
 *
 * SECTION: Actions and Subroutines/freopen()
 */

#pragma D option quiet

dtrace:::BEGIN
{
	freopen("/dev/null");
	exit(0);
}
