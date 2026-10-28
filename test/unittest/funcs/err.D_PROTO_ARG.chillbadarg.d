/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	chill() should handle a bad argument.
 *
 * SECTION: Actions and Subroutines/chill()
 *
 */


dtrace:::BEGIN
{
	chill("badarg");
	exit(0);
}

