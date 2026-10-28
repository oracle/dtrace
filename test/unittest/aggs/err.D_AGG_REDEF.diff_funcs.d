/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: An aggregation cannot be redefined with a different function.
 *
 * SECTION: Aggregations/Aggregations
 */

dtrace:::BEGIN
{
	@a = count();
	exit(0);
}

dtrace:::END
{
	@a = max(0);
}
