#ifndef TREE_SITTER_ADAPTER_H
#define TREE_SITTER_ADAPTER_H

#include "dslsyntax_common.h"
#include "tree_sitter/api.h"

typedef enum TSAdapterLanguage {
    TS_ADAPTER_C,
    TS_ADAPTER_PYTHON,
    TS_ADAPTER_JAVASCRIPT
} TSAdapterLanguage;

typedef struct TSAdapterProfile {
    const char *name;
    TSAdapterLanguage language;
} TSAdapterProfile;

void dslsh_tree_sitter_parse(CodeBuffer *codeBuffer, const TSLanguage *language, const TSAdapterProfile *profile);

#endif
