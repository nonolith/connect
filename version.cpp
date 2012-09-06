const char* server_version="1.3";

#ifdef GITVERSION

#define stringify(x) #x
#define xstringify(x) stringify(x)

const char* server_git_version = xstringify(GITVERSION);

#else

const char* server_git_version = "unknown";

#endif
