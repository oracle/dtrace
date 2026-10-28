/*
 * Oracle Linux DTrace.
 * Copyright (c) 2012, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@trigger: none */

profile:::profile-10ms
{
	trace(chip);
	n++;
}

profile:::profile-10ms
/n > 2/
{
	exit(0);
}

profile:::tick-1s
{
	exit(1);
}
