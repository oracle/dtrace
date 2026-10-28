/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: // comments are supported.
 */

// exit(1);
dtrace:::BEGIN // exit(1);
{ // exit(1);
	exit(0); // exit(1);
} // exit(1);
