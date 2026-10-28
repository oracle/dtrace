/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	Multiple BEGIN providers
 *
 * SECTION: dtrace Provider
 *
 */


#pragma D option quiet

dtrace:::BEGIN
{
	printf("Begin fired first\n");
}
dtrace:::BEGIN
{
	printf("Begin fired second\n");
}
dtrace:::BEGIN
{
	printf("Begin fired third\n");
}
dtrace:::BEGIN
{
	printf("Begin fired fourth\n");
}
dtrace:::BEGIN
{
	printf("Begin fired fifth\n");
	exit(0);
}
