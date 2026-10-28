/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  Test tracemem() with too few arguments.
 *
 * SECTION: Actions and Subroutines/tracemem()
 */


dtrace:::BEGIN
{
	tracemem(123);
}
