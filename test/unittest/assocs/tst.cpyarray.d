/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *
 * Copy value from associative arrays to local variables.
 *
 * SECTION: Variables/Associative Arrays
 *
 *
 */

#pragma D option quiet

this int x;

dtrace:::BEGIN
{
	a["abc", 123] = 123;
}

profile:::tick-10ms
{
	this->x = a["abc", 123]++;
	printf("The value of x is %d\n", this->x);
}

profile:::tick-10ms
{
	this->x = a["abc", 123]++;
	printf("The value of x is %d\n", this->x);
}

profile:::tick-10ms
{
	this->x = a["abc", 123]++;
	printf("The value of x is %d\n", this->x);
	exit(0);
}

dtrace:::ERROR
{
	exit(1);
}
