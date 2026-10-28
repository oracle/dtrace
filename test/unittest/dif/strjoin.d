dtrace:::BEGIN
{
	trace(strjoin(probeprov, probename));
	exit(0);
}

dtrace:::ERROR
{
	exit(1);
}
