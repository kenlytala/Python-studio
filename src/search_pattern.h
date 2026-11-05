#ifndef PYTHON_STUDIO_SEARCH_PATTERN_H
#define PYTHON_STUDIO_SEARCH_PATTERN_H
#include "folder_search.h"
typedef struct SearchMatcher SearchMatcher;
bool folder_search_options_valid(const SearchOptions *options, char *error, size_t cap);
SearchMatcher *search_matcher_new(const char *query, const SearchOptions *options, const atomic_bool *cancel,
                                  char *error, size_t cap);
void search_matcher_free(SearchMatcher *matcher);
bool search_matcher_regex(const SearchMatcher *matcher);
void search_matcher_begin(SearchMatcher *matcher);
/* -1 error, 0 no match/cancel, 1 match; spans are UTF-8 byte boundaries. */
int search_matcher_next(SearchMatcher *matcher, const char *text, size_t len, size_t start, size_t *begin,
                        size_t *end, char *error, size_t cap);
/* kind: 0 include files, 1 exclude files, 2 include directories, 3 exclude dirs. */
int search_matcher_filter(SearchMatcher *matcher, unsigned kind, const char *relative, bool directory,
                          char *error, size_t cap);
#endif
