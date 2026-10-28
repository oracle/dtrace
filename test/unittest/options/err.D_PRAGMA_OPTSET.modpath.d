/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Option modpath cannot be used from within a D program.
 *
 * SECTION: Options and Tunables/Consumer Options
 */

#pragma D option modpath=/nonexistent/directory

dtrace:::BEGIN
{
	exit(0);
}
