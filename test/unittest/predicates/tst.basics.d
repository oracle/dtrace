/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * 	Simple if,then operations test.
 *	Call simple expressions and make sure test satisfy conditions.
 *	Match expected output in tst.basics.d.out
 *
 * SECTION: Program Structure/Predicates
 *
 */

#pragma D option quiet

dtrace:::BEGIN
{
	i = 0;
}

profile:::tick-10ms
/i < 10/
{
	i++;
	trace(i);
}

profile:::tick-10ms
/i == 10/
{
	exit(0);
}
