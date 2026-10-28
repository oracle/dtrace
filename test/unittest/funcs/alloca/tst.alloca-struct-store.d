/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Storing to struct members works for alloca pointers.
 *
 * SECTION: Actions and Subroutines/alloca()
 */

#pragma D option quiet

struct foo {
	int	a;
	char	b;
	int	c;
} *ptr;

dtrace:::BEGIN
{
	ptr = alloca(sizeof(struct foo));
	ptr->c = 0x42;
	exit(0);
}

dtrace:::ERROR
{
	exit(1);
}
