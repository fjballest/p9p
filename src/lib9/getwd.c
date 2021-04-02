#include <u.h>
#include <libc.h>

#undef getwd

char*
p9getwd(char *s, int ns)
{
	s = getcwd(s, ns);
	if(s != nil && strncmp(s, "/Users/nemo/u", 13) == 0 &&
		(s[13] == 0 || s[13] == '/'))
		memmove(s, s+11, strlen(s)+1);
	return s;
}
