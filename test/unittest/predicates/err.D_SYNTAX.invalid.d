/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * 	Simple if,then operation test.
 *	Call invalid conditions and make sure it gives compilation error.
 *
 * SECTION: Program Structure/Predicates
 *
 */

#pragma D option quiet

dtrace:::BEGIN
{
	i = 0;
}

profile:::profile-1
/i < != > 10/
{
	i++;
	trace(i);
}
