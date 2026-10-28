/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Using copyinstr() with a NULL source address reports a fault.
 *
 * SECTION: Actions and Subroutines/copyinstr()
 *	    User Process Tracing/copyin() and copyinstr()
 */

dtrace:::BEGIN
{
	copyinstr(0, 8);
	exit(0);
}

dtrace:::ERROR
{
	exit(1);
}
