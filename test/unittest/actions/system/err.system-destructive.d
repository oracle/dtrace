/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: system() is a destructive action
 *
 * SECTION: Actions and Subroutines/system()
 */

#pragma D option quiet

dtrace:::BEGIN
{
	system("echo ok");
	exit(0);
}
