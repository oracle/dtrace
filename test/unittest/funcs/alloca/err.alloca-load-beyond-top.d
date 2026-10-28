/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Loads from beyond the top of alloca()ed memory fail.
 *            (Small overreads will often succeed due to alignment
 *            guarantees.  This one should always fail.)
 *
 * SECTION: Actions and Subroutines/alloca()
 */

#pragma D option quiet

dtrace:::BEGIN
{
	s = (char *)alloca(15);
	trace(s[16]);
        exit(0);
}

dtrace:::ERROR
{
	exit(1);
}
