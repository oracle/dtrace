/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * 	END without BEGIN profile
 *
 * SECTION: dtrace Provider
 *
 */


#pragma D option quiet

dtrace:::END
{
	printf("End fired after exit\n");
}

profile:::tick-1
{
	exit(0);
}
