dtrace:::BEGIN {
	trace(pid);
	exit(0);
}
