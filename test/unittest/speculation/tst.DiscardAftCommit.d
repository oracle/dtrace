/*
 * Oracle Linux DTrace.
 * Copyright (c) 2007, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Can call discard() on a buffer after it has been committed.
 *
 * SECTION: Speculative Tracing/Discarding a Speculation
 *
 */

#pragma D option quiet

dtrace:::BEGIN
{
	self->i = 0;
	self->commit = 0;
	self->discard = 0;
	var1 = speculation();
	printf("Speculation ID: %d\n", var1);
}

dtrace:::BEGIN
/var1/
{
	speculate(var1);
	printf("This statement and the following are speculative!!\n");
	printf("Speculating on id: %d\n", var1);
	self->i++;
}

dtrace:::BEGIN
/(self->i)/
{
	commit(var1);
	self->commit++;
	discard(var1);
	self->discard++;
}

dtrace:::BEGIN
/self->discard/
{
	printf("Discarded a committed buffer\n");
	exit(0);
}


dtrace:::BEGIN
/!self->discard/
{
	printf("Couldn't discard a committed buffer\n");
	exit(1);
}

dtrace:::ERROR
{
	exit(1);
}
