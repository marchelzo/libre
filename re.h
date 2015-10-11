#include <stdbool.h>

struct re_nfa;
typedef struct re_nfa re_pat;

re_pat *  re_compile (char const *);
bool      re_match   (re_pat const *, char const *);
void      re_free    (re_pat *);

