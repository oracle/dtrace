/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The -xdefaultargs option ensures that undefined arguments are 0.
 *
 * SECTION: Options and Tunables/Consumer Options
 */

/* @@runtest-opts: -xdefaultargs */

dtrace:::BEGIN
{
	exit($1);
}

dtrace:::ERROR
{
	exit(1);
}
