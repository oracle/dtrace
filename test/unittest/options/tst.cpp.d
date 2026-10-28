/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The -xcpp option enables pre-processing of the input file.
 *
 * SECTION: Options and Tunables/Consumer Options
 */

/* @@runtest-opts: -xcpp */

dtrace:::BEGIN
{
	exit(0);
}

dtrace:::ERROR
{
	exit(1);
}

#if 0
#endif
