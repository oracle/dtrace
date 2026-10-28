/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Without -w or -xdestructive, destructive operations are not ok.
 *
 * SECTION: Options and Tunables/Consumer Options
 */

dtrace:::BEGIN
{
	system("echo ok");
	exit(0);
}
