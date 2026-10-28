/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Simple denormalization test
 *
 * SECTION: Aggregations/Data Normalization
 *
 */

#pragma D option quiet

dtrace:::BEGIN
{
	i = 0;
}

profile:::tick-10ms
/i < 20/
{
	@a = sum(i * 100);
	@b = sum(i * 100);
	i++;
}

profile:::tick-10ms
/i == 20/
{
	normalize(@a, 5);
	denormalize(@a);
	exit(0);
}
