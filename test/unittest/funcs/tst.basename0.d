/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet

dtrace:::BEGIN
{
	printf("|%s|\n", basename("/foo/bar/baz"));
	printf("|%s|\n", basename("/foo/bar///baz/"));
	printf("|%s|\n", basename("/foo/bar/baz/"));
	printf("|%s|\n", basename("/foo/bar/baz//"));
	printf("|%s|\n", basename("/foo/bar/baz/."));
	printf("|%s|\n", basename("/foo/bar/baz/./"));
	printf("|%s|\n", basename("/foo/bar/baz/.//"));
	printf("|%s|\n", basename("foo/bar/baz/"));
	printf("|%s|\n", basename("/"));
	printf("|%s|\n", basename("./"));
	printf("|%s|\n", basename("//"));
	printf("|%s|\n", basename("/."));
	printf("|%s|\n", basename("/./"));
	printf("|%s|\n", basename("/./."));
	printf("|%s|\n", basename("/.//"));
	printf("|%s|\n", basename("."));
	printf("|%s|\n", basename("f"));
	printf("|%s|\n", basename("f/"));
	printf("|%s|\n", basename("/////"));
	printf("|%s|\n", basename(""));
	exit(0);
}

dtrace:::ERROR
{
	exit(1);
}
