/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: strlen() obeys the maximum string size setting
 *
 * SECTION: Actions and Subroutines/strlen()
 */

#pragma D option strsize=6
#pragma D option quiet

dtrace:::BEGIN
{
	exit(strlen("123456789") == 5 ? 0 : 1);
}

dtrace:::ERROR
{
	exit(1);
}
