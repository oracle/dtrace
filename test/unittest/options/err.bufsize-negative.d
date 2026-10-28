/*
 * Oracle Linux DTrace.
 * Copyright (c) 2024, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The -xbufsize option sets the trace buffer size.
 *
 * SECTION: Options and Tunables/Consumer Options
 */

/* @@runtest-opts: -xbufsize=-1 */

dtrace:::BEGIN
{
	exit(0);
}

dtrace:::ERROR
{
	exit(1);
}
