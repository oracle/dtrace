/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The 'lgrp' inline variable returns the correct value.
 */

#pragma D option quiet

dtrace:::BEGIN
{
	trace(lgrp);
}

dtrace:::BEGIN
/lgrp == curcpu->cpu_lgrp/
{
	exit(0);
}

dtrace:::BEGIN,
dtrace:::ERROR {
	exit(1);
}
