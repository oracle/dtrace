/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Calling alloca(0) is valid and subsequent calls will give the
 *	      same result.
 *
 * SECTION: Actions and Subroutines/alloca()
 */

#pragma D option quiet

dtrace:::BEGIN
{
	exit(alloca(0) == alloca(0) ? 0 : 1);
}

dtrace:::ERROR
{
	exit(1);
}
