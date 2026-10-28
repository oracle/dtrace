/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The normalize() action requires 2 arguments (agg and normal).
 *
 * SECTION: Aggregations/Data Normalization
 */

#pragma D option quiet

dtrace:::BEGIN
{
	@a = count();
	normalize(@a);
	exit(0);
}
