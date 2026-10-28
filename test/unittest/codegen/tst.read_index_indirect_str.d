/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet

char	*s;

dtrace:::BEGIN
{
	s = probeprov;			/* "dtrace", so s[0] better be 'd' */
	exit(s[0] == 'd' ? 0 : 1);
}

dtrace:::ERROR
{
	exit(1);
}
