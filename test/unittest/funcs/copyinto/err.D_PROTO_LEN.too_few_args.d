/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Three arguments are required for copyinto().
 *
 * SECTION: Actions and Subroutines/copyinto()
 */

dtrace:::BEGIN
{
	copyinto(1, 2);
	exit(1);
}

dtrace:::ERROR
{
	exit(0);
}
