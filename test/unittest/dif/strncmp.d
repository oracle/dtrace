dtrace:::BEGIN
{
	rc = probename == "BEGIN" ? 0 : 1;
	printf("%d", rc);

	exit(rc);
}

dtrace:::END
{
	printf("%d", probename == "BEGIN" ? 0 : 1);
}

