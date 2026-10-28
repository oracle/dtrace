/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Destructive actions may never be speculative.
 *
 * SECTION: Speculative Tracing/Using a Speculation
 */
#pragma D option destructive
#pragma D option quiet

char a[2];
void *buf;
uintptr_t addr;
size_t nbytes;
dtrace:::BEGIN
{
	self->i = 0;
	addr = (uintptr_t)&a[0];
	nbytes = 10;
	var = speculation();
}

dtrace:::BEGIN
{
	speculate(var);
	printf("Speculation ID: %d", var);
	self->i++;
	copyout(buf, addr, nbytes);
}

dtrace:::BEGIN
{
	printf("This test should not have compiled\n");
	exit(0);
}

dtrace:::ERROR
{
	exit(0);
}
