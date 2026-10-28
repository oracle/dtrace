/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: max() should not more than one argument
 *
 * SECTION: Aggregations/Aggregations
 */

dtrace:::BEGIN
{
	@ = max(1, 2);
	exit(0);
}
