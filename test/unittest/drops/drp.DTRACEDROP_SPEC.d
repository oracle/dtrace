/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option nspec=1
#pragma D option specsize=32
#pragma D option strsize=512

dtrace:::BEGIN
{
	spec = speculation();
	speculate(spec);
	trace("The, SystemTap, The.");
}

dtrace:::BEGIN
{
	commit(spec);
}

dtrace:::BEGIN
{
	exit(0);
}
