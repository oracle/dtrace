/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

dtrace:::BEGIN
{
}

syscall::write:entry
{
	exit(1);
}

dtrace:::END
{
	exit(0);
}
