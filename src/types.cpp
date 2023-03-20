// Macro magic for supported disk types

// First include types as declarations.
#include "types.h"
#define DEFINITIONS_FOR_CPP
// Then include types as definitions, and that is the content of this file.
#include "types.h"
#undef DEFINITIONS_FOR_CPP
