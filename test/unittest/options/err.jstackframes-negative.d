/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The -xjstackframes option requires a non-negative value
 *
 * SECTION: Options and Tunables/Consumer Options
 */

/* @@runtest-opts: -xjstackframes=-1 */

dtrace:::BEGIN
{
	exit(0);
}

dtrace:::ERROR
{
	exit(1);
}
