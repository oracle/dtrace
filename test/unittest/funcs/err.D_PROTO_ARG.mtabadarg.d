/*
 * Oracle Linux DTrace.
 * Copyright (c) 2007, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	mutex_type_adaptive() should handle an invalid argument passed.
 *
 * SECTION: Actions and Subroutines/mutex_type_adaptive()
 *
 */


dtrace:::BEGIN
{
	mutex_type_adaptive(trace("badarg"));
	exit(1);
}
