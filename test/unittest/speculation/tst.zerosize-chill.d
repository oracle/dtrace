/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@xfail: dtv2, chill not implemented yet */

#pragma D option destructive

dtrace:::BEGIN
{
	self->spec = speculation();
	speculate(self->spec);
}

dtrace:::BEGIN
{
	this->one = 1;
	this->two = 2;
	chill(1);
	speculate(self->spec);
}

dtrace:::BEGIN
{
	speculate(self->spec);
}

dtrace:::BEGIN
{
	exit(0);
}
