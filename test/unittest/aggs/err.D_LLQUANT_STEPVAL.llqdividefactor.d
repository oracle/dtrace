/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  llquantize() steps must divide factor when steps<factor
 *
 * SECTION: Aggregations/Aggregations
 *
 */


dtrace:::BEGIN
{
	@ = llquantize(1234, 10, 0, 6, 3);
}
