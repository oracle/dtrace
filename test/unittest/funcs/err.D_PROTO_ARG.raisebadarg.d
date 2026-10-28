/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  raise() should generate an error if any arguments are sent.
 *
 * SECTION: Actions and Subroutines/raise()
 *
 */


dtrace:::BEGIN
{
	raise("badarg");
	exit(0);
}

