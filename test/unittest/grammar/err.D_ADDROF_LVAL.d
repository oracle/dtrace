/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	& can not be applied to a non-lvalue
 *
 * SECTION: Pointers and Arrays/Pointers and Addresses
 *
 */

dtrace:::BEGIN
{
	trace(&1);
	exit(0);
}
