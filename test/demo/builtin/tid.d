dtrace:::BEGIN {
	trace(tid);
	exit(0);
}
