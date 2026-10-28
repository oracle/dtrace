/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Test the use of a non-supported aggregate function.
 *
 * SECTION: Aggregations/Aggregations
 */

dtrace:::BEGIN
{
	@ = f();
	exit(0);
}
