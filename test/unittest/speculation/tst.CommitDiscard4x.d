/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: A clause can have multiple commits and discards.
 *
 * SECTION: Speculative Tracing/Committing a Speculation
 *
 */
#pragma D option quiet
#pragma D option nspec=8

dtrace:::BEGIN
{
	a = speculation();
	b = speculation();
	c = speculation();
	d = speculation();
	e = speculation();
	f = speculation();
	g = speculation();
	h = speculation();
	printf("Speculation IDs: %d %d %d %d %d %d %d %d\n",
	       a, b, c, d, e, f, g, h);
}

dtrace:::BEGIN
{
	speculate(a);
	printf("Speculating on id: %d\n", a);
}

dtrace:::BEGIN
{
	speculate(b);
	printf("Speculating on id: %d\n", b);
}

dtrace:::BEGIN
{
	speculate(c);
	printf("Speculating on id: %d\n", c);
}

dtrace:::BEGIN
{
	speculate(d);
	printf("Speculating on id: %d\n", d);
}

dtrace:::BEGIN
{
	speculate(e);
	printf("Speculating on id: %d\n", e);
}

dtrace:::BEGIN
{
	speculate(f);
	printf("Speculating on id: %d\n", f);
}

dtrace:::BEGIN
{
	speculate(g);
	printf("Speculating on id: %d\n", g);
}

dtrace:::BEGIN
{
	speculate(h);
	printf("Speculating on id: %d\n", h);
}

dtrace:::BEGIN
{
	commit(h);
	discard(g);
	commit(f);
	discard(e);
	commit(d);
	discard(c);
	commit(b);
	discard(a);
}

dtrace:::BEGIN
{
	exit(0);
}

dtrace:::ERROR
{
	exit(1);
}
