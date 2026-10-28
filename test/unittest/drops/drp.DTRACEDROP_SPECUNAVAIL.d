/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

#pragma D option nspec=1

dtrace:::BEGIN,
dtrace:::BEGIN,
dtrace:::BEGIN,
dtrace:::BEGIN
{
	spec = speculation();
}

dtrace:::BEGIN
{
	exit(0);
}
