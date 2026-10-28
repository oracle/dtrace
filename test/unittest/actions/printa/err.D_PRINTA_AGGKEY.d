/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/*
 * ASSERTION: printa() requires aggregation arguments to have matching key
 *	signatures
 *
 * SECTION: Output Formatting/printa()
 */

dtrace:::BEGIN
{
	@a = count();
	@b[1] = count();
	printa(@a, @b);
	exit(0);
}
