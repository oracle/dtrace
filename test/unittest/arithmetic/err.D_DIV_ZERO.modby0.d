/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *   Verify that mod by 0 errors are caught
 *
 * SECTION:
 *	Types, Operators, and Expressions/Arithmetic Operators
 */



dtrace:::BEGIN
{
	c = 123%0;
	exit(0);
}

