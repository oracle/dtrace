/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: A clause can have multiple commits.
 *
 * SECTION: Speculative Tracing/Committing a Speculation
 *
 */
#pragma D option quiet
#pragma D option nspec=3

dtrace:::BEGIN
{
	i = speculation();
	j = speculation();
	k = speculation();
	printf("Speculation IDs: %d %d %d\n", i, j, k);
}

dtrace:::BEGIN
{
	speculate(i);
	printf("Speculating on id: %d\n", i);
}

dtrace:::BEGIN
{
	speculate(j);
	printf("Speculating on id: %d\n", j);
}

dtrace:::BEGIN
{
	speculate(k);
	printf("Speculating on id: %d\n", k);
}

dtrace:::BEGIN
{
	commit(k);
	commit(j);
	commit(i);
}

dtrace:::BEGIN
{
	exit(0);
}

dtrace:::ERROR
{
	exit(1);
}
