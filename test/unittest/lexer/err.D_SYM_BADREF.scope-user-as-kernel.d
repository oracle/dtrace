/*
 * Oracle Linux DTrace.
 * Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@trigger: testprobe */

/*
 * ASSERTION: A registered executable's symbols cannot use kernel scoping.
 */

#pragma D option quiet

dtrace:::BEGIN
{
	trace(&testprobe`main);
	exit(0);
}
