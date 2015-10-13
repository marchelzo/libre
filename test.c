#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "re.h"

int main(int argc, char **argv)
{
        re_pat *re;
        char buffer[512];
        
        re = re_compile("this (is|was) fun");

        struct { char const *s; bool e; } tests[] = {
                { "this is fun", true }
              , { "this was fun", true }
              , { "this fun", false }
              , { "thi was is fun", false }
        };

        struct re_result m;
        for (size_t i = 0; i < sizeof tests / sizeof tests[0]; ++i) {
                if (re_match(re, tests[i].s, &m) != tests[i].e) {
                        fprintf(stderr, "Test failed: `%s`\n", tests[i].s);
                }
        }

        re_free(re);

        if (argc <= 1) {
                return 0;
        }

        re = re_compile(argv[1]);
        if (re == NULL) {
                fprintf(stderr, "Failed to compile regular expression: `%s`\n", argv[1]);
                return 1;
        }

        while (fgets(buffer, sizeof buffer, stdin)) {
                buffer[strcspn(buffer, "\n")] = 0;
                if (re_match(re, buffer, &m)) {
                        printf("Match: `%.*s`\n", (int) (m.end - m.start), m.start);
                } else {
                        puts("No match!");
                }
        }

        re_free(re);

        return 0;
}
