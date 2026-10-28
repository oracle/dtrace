dtrace:::BEGIN {
	trace(execname);
	exit(0);
}
