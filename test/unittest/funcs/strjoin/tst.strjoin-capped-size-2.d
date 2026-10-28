/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option rawbytes
#pragma D option strsize=7
#pragma D option quiet

dtrace:::BEGIN
{
	x = probeprov;
	y = probename;
	z = strjoin(x, y);

	trace(x);
	trace(y);
	trace(z);

	exit(0);
}

dtrace:::ERROR
{
	exit(1);
}
