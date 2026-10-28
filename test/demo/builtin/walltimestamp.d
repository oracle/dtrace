dtrace:::BEGIN {
	trace(walltimestamp);
	exit(0);
}
