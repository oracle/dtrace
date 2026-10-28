/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Using copyinto() with an invalid source address reports a fault.
 *
 * SECTION: Actions and Subroutines/copyinto()
 */

dtrace:::BEGIN
{
	ptr = (char *)alloca(256);
	copyinto(0x1234, 8, ptr);
	exit(0);
}

dtrace:::ERROR
{
	exit(1);
}
