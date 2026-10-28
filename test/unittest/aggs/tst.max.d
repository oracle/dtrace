/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Positive max() test
 *
 * SECTION: Aggregations/Aggregations
 *
 * NOTES: This is verifiable simple positive test of the max() function.
 */

#pragma D option quiet

dtrace:::BEGIN
{
	i = 100;
}

profile:::tick-10ms
/i < 1000/
{
	@a = max(i);
	i += 100;
}

profile:::tick-10ms
/i == 1000/
{
	printa(@a);
	exit(0);
}
