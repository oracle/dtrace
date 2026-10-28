/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Casting a curcpu value to a scalar is allowed.
 *
 * SECTION: Types, Operators, and Expressions/Arithmetic Operators
 */

#pragma D option quiet

dtrace:::BEGIN
{
	val = ((int64_t)curcpu) >> 8;
	trace(val);
	exit(0);
}

dtrace:::ERROR
{
	exit(1);
}
