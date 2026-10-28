/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet

int *p;

dtrace:::BEGIN
{
	p = alloca(sizeof(int) * 4);
	p[3] = htonl(0x7f000001);
	trace(inet_ntoa6(p));

	exit(0);
}
