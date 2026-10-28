/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Passing NULL (0) as argument to strlen() triggers an error.
 *
 * SECTION: Actions and Subroutines/strlen()
 */

#pragma D option quiet

dtrace:::BEGIN
{
	strlen(0);
	exit(1);
}

dtrace:::ERROR
{
	exit(0);
}
