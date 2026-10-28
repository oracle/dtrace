/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

dtrace:::BEGIN
{
	trace(strtok("abc", "def"));
}

dtrace:::BEGIN
{
	trace(strtok(NULL, "def"));
}

dtrace:::BEGIN
{
	exit(1);
}

dtrace:::ERROR
{
	exit(0);
}
