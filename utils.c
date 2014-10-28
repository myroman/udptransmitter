void rmnl(char* s) {
	int ln = strlen(s) - 1;
	if (s[ln] == '\n')
	    s[ln] = '\0';
}