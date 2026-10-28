/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: If aggsize is too small to hold a single aggregation, report an
 *	      error.
 *
 * SECTION: Aggregations/Aggregations
 */

#pragma D option aggsize=8

dtrace:::BEGIN
{
	@ = count();
	exit(0);
}

dtrace:::ERROR
{
	exit(0);
}
