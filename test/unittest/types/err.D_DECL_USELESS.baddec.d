/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	Bad external declarations will result in a D_DECL_USELESS
 *	error.
 *
 * SECTION: Types, Operators, and Expressions/Data Types and Sizes
 *
 */

extern int;

dtrace:::BEGIN
{
	exit(1);
}
