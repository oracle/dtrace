/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: alloca does not live beyond clause boundaries.
 *
 * SECTION: Actions and Subroutines/alloca()
 */

#pragma D option quiet

dtrace:::BEGIN
{
	s = (char *)alloca(1);
        *s = 'a';
}

dtrace:::BEGIN
{
        trace(*s);
        exit(0);
}

dtrace:::ERROR
{
	exit(1);
}