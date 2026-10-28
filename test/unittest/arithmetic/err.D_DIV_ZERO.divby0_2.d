/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *   Verify that computed divide by 0 errors are caught
 *
 * SECTION:
 *	Types, Operators, and Expressions/Arithmetic Operators
 */





dtrace:::BEGIN
{
	c = 123/((7-7) * 999);
	exit(0);
}

