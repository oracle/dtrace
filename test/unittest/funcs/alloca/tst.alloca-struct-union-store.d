/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Storing to nested members works for alloca pointers.
 *
 * SECTION: Actions and Subroutines/alloca()
 */

#pragma D option quiet

struct st {
    struct {
	union {
	    struct {
		int	a;
		char	b;
		int	c;
	    } apa;
	    uint64_t	val;
	} u;
    } s;
} *ptr;

dtrace:::BEGIN
{
	ptr = alloca(sizeof(struct st));
	ptr->s.u.apa.c = 0x42;
	exit(0);
}

dtrace:::ERROR
{
	exit(1);
}
