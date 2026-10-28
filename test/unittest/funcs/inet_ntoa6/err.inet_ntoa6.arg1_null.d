/*
 * Oracle Linux DTrace.
 * Copyright (c) 2007, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet

struct in6_addr *ip6a;

dtrace:::BEGIN
{
	ip6a = 0;

	printf("%s\n", inet_ntoa6(ip6a));

	exit(0);
}

dtrace:::ERROR
{
	exit(1);
}
