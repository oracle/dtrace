/*
 * Oracle Linux DTrace.
 * Copyright (c) 2024, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:  casting keeps a variable writable.
 *
 * SECTION: Types, Operators, and Expressions/Constants
 */

#pragma D option quiet

dtrace:::BEGIN
{
	x = (char *) alloca(8);
	*((long long*)x) = 0x0067666564636261;
	trace(stringof(x));
	exit(0);
}
