libre
=====

#### libre is a tiny regular expression library for C

libre compiles regular expressions into NFAs for reasonably
fast matching speeds. Its interface is very simple; it exposes
three functions, all of which are self-explanatory.

```c
re_pat *re_compile(char const *)

bool re_match(re_pat const *, char const *)

void re_free(re_pat *)
```

#### Usage

To use libre, simply clone this repository and run `make && sudo make install`.


#### Example

```c
#include <re.h>
#include <stdio.h>
#include <assert.h>

int main(void)
{
        re_pat *p = re_compile("(https?://)?www.google.(ca|com)");
        if (p == NULL) {
                fputs("Failed to compile regular expression.\n", stderr);
                return 1;
        }

        assert(re_match(p, "https://www.google.ca"));
        assert(re_match(p, "http://www.google.ca"));
        assert(re_match(p, "https://www.google.com"));
        assert(re_match(p, "https://www.google.com"));
        assert(re_match(p, "www.google.ca"));
        assert(re_match(p, "www.google.com"));

        assert(!re_match(p, "www.github.com"));
        assert(!re_match(p, "http://google.com"));
        assert(!re_match(p, "http://google.ca"));

        re_free(p);

        return 0;
}
```

```
$ cc -std=c11 -o example example.c -lre
$ ./example
$
```
