#ifndef _cfglib_h_
#define _cfglib_h_

#include <stdio.h>

typedef enum {
	ct_string,
	ct_bool,
	ct_int,
	ct_toggle,
	ct_sub
} cfglib_type_t;

typedef struct cfglib_option_t cfglib_option_t;
typedef union cfglib_data_t cfglib_data_t;
typedef struct cfglib_list_t cfglib_list_t;
typedef struct cfglib_value_t cfglib_value_t;
typedef void (*cfglib_callback_t)(cfglib_value_t *value);
typedef struct cfglib_value_list_t cfglib_value_list_t;

struct cfglib_list_t
{
	int items;
	int allocated;
	cfglib_data_t **data;
};

union cfglib_data_t
{
	int val_int;
	int val_bool;
	int val_toggle;
	char *val_string;
	cfglib_list_t val_list;
	cfglib_value_list_t *val_sub;
};

struct cfglib_value_t
{
	char *name;
	cfglib_data_t *data;
	cfglib_option_t *opt;
};

struct cfglib_option_t
{
	const char *name;     /* this is the name of the config option */
	cfglib_type_t type;   /* type of variable (string, bool, etc.) */
	unsigned flags;
	cfglib_callback_t callback;  /* callback function */
	int data;             /* some user-defined data */
	cfglib_option_t *subopts;
	char *comment;
	struct cfglib_option_t *parent;
};

struct cfglib_value_list_t
{
	int items;
	int allocated;
	cfglib_value_t **values;
};

/* flags for cfglib_option_t->flags, can be or'ed together */
#define CFGF_LIST    (1 << 1)
#define CFGF_ALLOW_MULTIPLES (1 << 2)
#define CFGF_LIST_MERGE_MULTIPLES (1 << 3)

#define cfgftest(a, b)   (((a) & (b)) == (b))


#define CFG_OPT(name, type, flags, callback, index, subopts, comment) \
    {name, type, flags, callback, index, subopts, comment, 0}
#define CFG_SUB(name, flags, callback, index, subopts, comment) \
    CFG_OPT(name, ct_sub, flags, callback, index, subopts, comment)
#define CFG_STR(name, flags, callback, index, comment) \
    CFG_OPT(name, ct_string, flags, callback, index, 0, comment)
#define CFG_BOOL(name, flags, callback, index, comment) \
    CFG_OPT(name, ct_bool, flags, callback, index, 0, comment)
#define CFG_INT(name, flags, callback, index, comment) \
    CFG_OPT(name, ct_int, flags, callback, index, 0, comment)
#define CFG_TOGGLE(name, flags, callback, index, comment) \
    CFG_OPT(name, ct_toggle, flags, callback, index, 0, comment)
#define CFG_ENDOPT {(char *)-1, 0, 0, 0, 0, 0, 0}

#define cfglib_getstring(n, v) ((char *)cfglib_getopt((n), (v)))
#define cfglib_getbool(n, v) ((int)cfglib_getopt((n), (v)))
#define cfglib_getint(n, v) ((int)cfglib_getopt((n), (v)))
#define cfglib_gettoggle(n, v) ((int)cfglib_getopt((n), (v)))
#define cfglib_getlist(n, v) ((cfglib_list_t *)cfglib_getopt((n), (v)))
#define cfglib_getsub(n, v) ((cfglib_value_list_t *)cfglib_getopt((n), (v)))

int cfglib_load(const char *filename, cfglib_option_t *opts,
				cfglib_value_list_t **values);
int cfglib_save(const char *filename, cfglib_value_list_t *values);

void *cfglib_getopt(const char *name, cfglib_value_list_t *values);

void cfglib_free_value(cfglib_value_t *value);
void cfglib_free_values(cfglib_value_list_t *values);

void cfglib_trim_end(char *str, char *trim, int repeat);
int cfglib_str2bool(const char *e);
char *cfglib_read_string(FILE *fp, char *parameter);
char *cfglib_nextstr(FILE *fp);

char *cfglib_env_expand(char *parameter);

#endif
