#ifndef _lglob_h_included
#define _lglob_h_included

#include "linklist.h"

typedef bool (*lglobfunc)(char *f);

list *lglob_create(void);
void lglob_destroy(list *gl);
bool lglob_exclude_dotdirs(char *f);
int lglob_glob(list *gl, const char *mask, lglobfunc exclude_func);

#endif
