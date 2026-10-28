/* @@skip: Linux has no zones. */

dtrace:::BEGIN {
	trace(zonename);
	exit(0);
}
