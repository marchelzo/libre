#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <assert.h>

#include <vec.h>

#include "re.h"

enum {
        NFA_EPSILON = UINT16_MAX,
        NFA_ANYCHAR = UINT16_MAX - 1,
        NFA_BEGIN   = UINT16_MAX - 2,
        NFA_END     = UINT16_MAX - 3
};

struct re {
        enum {
                RE_CHAR,
                RE_ALT,
                RE_STAR,
                RE_PLUS,
                RE_OPTION,
                RE_DOT,
                RE_CONCAT,
                RE_RANGE
        } type;

        union {
                uint8_t c;

                struct {
                        uint8_t low;
                        uint8_t high;
                };

                struct {
                        struct re *left;
                        struct re *right;
                };

                struct re *re;

                vec(struct re *) res;
        };
};

struct st {
        struct {
                struct st *s;
                uint16_t t;
        } one, two;
};


struct re_nfa {
        vec(struct st *) states;
};

/*
 * Attempt to allocate a new `struct re`.
 * Return a null-pointer if the attempt fails.
 */
#define mkre(name) \
        name = malloc(sizeof *name); \
        if (name == NULL) return NULL;


size_t
addstate(struct re_nfa *nfa)
{
        struct st *state, **out;
        state = malloc(sizeof *state);

        assert(state != NULL);

        state->one.s = NULL;
        state->two.s = NULL;

        vec_push(nfa->states, state, out);

        return nfa->states.size - 1;
}

static void
transition(struct re_nfa *nfa, size_t from, size_t to, uint16_t type)
{
        if (nfa->states.items[from]->one.s == NULL) {
                nfa->states.items[from]->one.s = nfa->states.items[to];
                nfa->states.items[from]->one.t = type;
        } else if (nfa->states.items[from]->two.s == NULL) {
                nfa->states.items[from]->two.s = nfa->states.items[to];
                nfa->states.items[from]->two.t = type;
        } else {
                assert(false);
        }
}


static size_t
tonfa(struct re_nfa *nfa, size_t start, struct re *re)
{
        size_t a, b, c;
        size_t t, v;

        switch (re->type) {
        case RE_CHAR:
                a = addstate(nfa);
                transition(nfa, start, a, re->c);
                return a;
        case RE_RANGE:
                a = addstate(nfa);
                transition(nfa, start, a, (re->low << 8) + re->high);
                return a;
        case RE_ALT:
                /* End state */
                c = addstate(nfa);
                
                /* Start of left */
                a = addstate(nfa);
                /* Start of right */
                b = addstate(nfa);

                /* End of left */
                t = tonfa(nfa, a, re->left);
                /* End of right */
                v = tonfa(nfa, b, re->right);

                /* Link left to end */
                transition(nfa, t, c, NFA_EPSILON);
                /* Link right to end */
                transition(nfa, v, c, NFA_EPSILON);

                /* Link start to left */
                transition(nfa, start, a, NFA_EPSILON);
                /* Link start to right */
                transition(nfa, start, b, NFA_EPSILON);

                return c;
        case RE_STAR:
                /* End state */
                b = addstate(nfa);

                /* Star's operand */
                a = addstate(nfa);

                /* End of star's operand */
                t = tonfa(nfa, a, re->re);

                /* Make the loop (connect end to start) */
                transition(nfa, t, a, NFA_EPSILON);

                /* The other way out: the end state */
                transition(nfa, t, b, NFA_EPSILON);

                /* Link start to star's operand (match 1 or more) */
                transition(nfa, start, a, NFA_EPSILON);

                /* Link start directly to end (match 0 times) */
                transition(nfa, start, b, NFA_EPSILON);

                return b;
        case RE_PLUS:
                /* End state */
                b = addstate(nfa);

                /* Plus's operand */
                a = addstate(nfa);

                /* End of plus's operand */
                t = tonfa(nfa, a, re->re);

                /* Make the loop (connect end to start) */
                transition(nfa, t, a, NFA_EPSILON);

                /* The other way out: the end state */
                transition(nfa, t, b, NFA_EPSILON);

                /* Link start to plus's operand (match 1 or more) */
                transition(nfa, start, a, NFA_EPSILON);

                return b;
        case RE_OPTION:
                /* End state */
                b = addstate(nfa);

                /* ?'s operand */
                a = addstate(nfa);

                /* End of ?'s operand */
                t = tonfa(nfa, a, re->re);

                /* Link end of ?'s operand to the end state */
                transition(nfa, t, b, NFA_EPSILON);

                /* Link start directly to end (no match) */
                transition(nfa, start, b, NFA_EPSILON);

                /* Link start to ?'s operand (match) */
                transition(nfa, start, a, NFA_EPSILON);

                return b;
        case RE_DOT:
                /* End state */
                b = addstate(nfa);

                /* Link start to end */
                transition(nfa, start, b, NFA_ANYCHAR);

                return b;
        case RE_CONCAT:
                t = start;

                for (size_t i = 0; i < re->res.size; ++i) {
                        t = tonfa(nfa, t, re->res.items[i]);
                }

                return t;
        }
}


static struct re *regexp(char const **, bool allow_trailing);
static struct re *subexp(char const **);
static struct re *atom(char const **);
static struct re *charclass(char const **);

static void
freere(struct re *re)
{
        if (re->type == RE_ALT) {
                freere(re->left);
                freere(re->right);
        } else if (re->type == RE_CONCAT) {
                for (size_t i = 0; i < re->res.size; ++i) {
                        freere(re->res.items[i]);
                }
                vec_empty(re->res);
        }

        free(re);
}

static struct re *
or(struct re *left, struct re *right)
{
        if (left == NULL) {
                return right;
        } else if (right == NULL) {
                return left;
        }

        struct re *e;
        mkre(e);
        e->type  = RE_ALT;
        e->left  = left;
        e->right = right;
        return e;
}

static struct re *
regexp(char const **s, bool allow_trailing)
{
        struct re **out;
        struct re *re, *e;

        mkre(e);
        e->type = RE_CONCAT;
        vec_init(e->res);
        
        while (**s && **s != '|') {
                struct re *sub = subexp(s);
                if (sub == NULL) {
                        if (allow_trailing) {
                                return e;
                        } else {
                                return NULL;
                        }
                }

                vec_push(e->res, sub, out);
                if (out == NULL) {
                        return NULL;
                }
        }

        if (!**s) {
                return e;
        }

        *s += 1;

        mkre(re);
        re->type  = RE_ALT;
        re->left  = e;
        re->right = regexp(s, allow_trailing);

        if (re->right == NULL) {
                return NULL;
        }

        return re;
}

static struct re *
subexp(char const **s)
{
        struct re *re, *e = atom(s);
        if (e == NULL) {
                return NULL;
        }
        
        int type = -1;

        switch (**s) {
        case '*': ++*s; type = RE_STAR;   break;
        case '+': ++*s; type = RE_PLUS;   break;
        case '?': ++*s; type = RE_OPTION; break;
        default:                          break;
        }

        if (type == -1) {
                return e;
        }

        mkre(re);
        re->type = type;
        re->re   = e;

        return re;
}

static struct re *
atom(char const **s)
{
        if (**s == '\0' || **s == ')') {
                return NULL;
        }

        /*
         * If the next character is a left
         * parenthesis, then we must match
         * a `regexp`; otherwise, we match
         * one character.
         */
        if (**s == '(') {
                *s += 1;
                struct re *e = regexp(s, true);
                if (**s != ')') {
                        return NULL;
                }
                *s += 1;
                return e;
        } else if (**s == '.') {
                struct re *e;
                mkre(e);
                e->type = RE_DOT;
                *s += 1;
                return e;
        } else if (**s == '[') {
                *s += 1;
                return charclass(s);
        } else if (**s == '\\') {
                struct re *e;
                *s += 1;
                if (**s == '\0') {
                        /* The regular expression cannot end with a backslash */
                        return NULL;
                }
                mkre(e);
                e->type = RE_CHAR;
                e->c    = **s;
                *s += 1;
                return e;
        } else {
                struct re *e;
                mkre(e);
                e->type = RE_CHAR;
                e->c    = **s;
                *s += 1;
                return e;
        }
}

static struct re *
charclass(char const **s)
{
        if (**s == '\0') {
                return NULL;
        }

        struct re *e = NULL;
        struct re *c;
        for (size_t i = 0; **s != '\0'; ++i, ++*s) {
                if (**s == ']' && i != 0) {
                        break;
                }
                mkre(c);
                if ((*s)[1] == '-' && (*s)[2] != '\0' && (*s)[2] != ']') {
                        c->type = RE_RANGE;
                        c->low  = (*s)[0];
                        c->high = (*s)[2];
                        *s += 2;
                } else {
                        c->type = RE_CHAR;
                        c->c    = **s;
                }
                e = or(e, c);
                if (e == NULL) {
                        goto fail;
                }
        }
        if (**s == '\0') {
                goto fail;
        }
        *s += 1;
        return e;
fail:
        freere(e);
        return NULL;
}

static struct re *
parse(char const *s)
{
        return regexp(&s, false);
}

static bool
charmatch(char const **s, uint16_t t, char const *begin)
{
        if (t == NFA_EPSILON) {
                return true;
        }

        if (t == NFA_ANYCHAR) {
                *s += 1;
                return true;
        }

        if (t == NFA_BEGIN) {
                return *s == begin;
        }

        if (t == NFA_END) {
                return **s == '\0';
        }

        if (t > UINT8_MAX) {
                if (**s < (t >> 8) || **s > (t & 0xFF)) {
                        return false;
                }
                *s += 1;
                return true;
        }

        if (**s == t) {
                *s += 1;
                return true;
        }

        return false;
}

static bool
domatch(struct st const *state, char const *s, char const *begin)
{
        char const *save;

        if (state->one.s == NULL) {
                assert(state->two.s == NULL);
                return true;
        }

        save = s;

        if (charmatch(&s, state->one.t, begin) && domatch(state->one.s, s, begin)) {
                return true;
        }

        if (state->two.s != NULL) {
                return domatch(state->two.s, save, begin);
        }

        return false;
}

bool
re_match(struct re_nfa const *nfa, char const *s)
{
        if (domatch(nfa->states.items[0], s, s)) {
                return true;
        }

        while (*++s) {
                if (domatch(nfa->states.items[0], s, NULL)) {
                        return true;
                }
        }

        return false;
}

re_pat *
re_compile(char const *s)
{
        assert(s);

        struct re_nfa *nfa = malloc(sizeof *nfa);
        if (nfa == NULL) {
                return NULL;
        }

        vec_init(nfa->states);

        struct re *re = parse(s);
        if (re == NULL) {
                free(nfa);
                return NULL;
        }

        addstate(nfa);
        tonfa(nfa, 0, re);

        freere(re);

        return nfa;
}

void
re_free(struct re_nfa *nfa)
{
        for (size_t i = 0; i < nfa->states.size; ++i) {
                free(nfa->states.items[i]);
        }

        vec_empty(nfa->states);

        free(nfa);
}
