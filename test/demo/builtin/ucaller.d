dtrace:::BEGIN {
	trace(ucaller);
	exit(0);
}
