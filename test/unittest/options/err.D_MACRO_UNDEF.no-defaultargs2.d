/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: With no -xdefaultargs option, missing arguments cause an error.
 *
 * SECTION: Options and Tunables/Consumer Options
 */

/* @@runtest-opts: 2 1 */

dtrace:::BEGIN
{
	trace($1);
	trace($2);
	trace($3);
	exit($3 ? 1 : 0);
}

dtrace:::ERROR
{
	exit(0);
}
