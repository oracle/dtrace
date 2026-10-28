/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet

dtrace:::BEGIN
{
	printf("%x", getminor(0xabcdef0123456789ll));
	exit(0);
}

dtrace:::ERROR
{
	exit(1);
}
