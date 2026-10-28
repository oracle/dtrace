/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet

dtrace:::BEGIN
{
	exit(curthread->thread_pid->numbers[0].nr == pid ? 0 : 1);
}

dtrace:::ERROR
{
	exit(1);
}
