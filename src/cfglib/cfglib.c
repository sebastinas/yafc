#include <string.h>
#include <stdlib.h>
#include "cfglib.h"

static cfglib_value_t *find_value(const char *optname,
								  cfglib_value_list_t *values)
{
	int i;

	if(!values)
		return 0;

	for(i = 0; i < values->items; i++) {
		if(strcasecmp(optname, values->values[i]->name) == 0)
			return values->values[i];
	}
	return 0;
}

void *cfglib_getopt(const char *name, cfglib_value_list_t *values)
{
	cfglib_value_t *v = find_value(name, values);

	return v ? v->data->val_string : 0;
}

int cfglib_setopt(const char *name, void *value, cfglib_value_t *values)
{
	return 0;
}

static cfglib_option_t *find_opt(const char *name, cfglib_option_t *opts)
{
	int i;
	cfglib_option_t *default_opt = 0;

	for(i=0; opts[i].name != (char *)-1; i++) {
		if(opts[i].name == 0)
			default_opt = &opts[i];
		else if(strcasecmp(name, opts[i].name) == 0)
			return &opts[i];
	}
	return default_opt;
}

static void cfglib_setparent(cfglib_option_t *opts, cfglib_option_t *parent)
{
	int i;

	for(i = 0; opts[i].name; i++)
		opts[i].parent = parent;
}

static cfglib_value_t *alloc_value(void)
{
	cfglib_value_t *v = (cfglib_value_t *)malloc(sizeof(cfglib_value_t));
	memset(v, 0, sizeof(cfglib_value_t));
	return v;
}

static cfglib_data_t *parse_data(FILE *fp, cfglib_option_t *opt, char *parameter)
{
	cfglib_data_t *data = (cfglib_data_t *)malloc(sizeof(cfglib_data_t));

	switch(opt->type) {
	case ct_string:
		data->val_string = cfglib_read_string(fp, parameter);
		break;
	case ct_bool:
		data->val_bool = cfglib_str2bool(parameter);
		if(data->val_bool == -1) {
			fprintf(stderr,
					"%s: expected boolean value but got '%s'\n",
					opt->name, parameter);
			data->val_bool = 0;
		}
		break;
	case ct_int:
		data->val_int = atoi(parameter);
		break;
	case ct_sub:
		break;
	case ct_toggle:
		data->val_toggle = 1;
		break;
	}

	return data;
}

/* adds DATA to LIST, possibly reallocating LIST if necessary
 */
static void add_to_list(cfglib_list_t *list, cfglib_data_t *data)
{
	if(list->allocated <= list->items) {
		list->allocated += 10;
		list->data = (cfglib_data_t **)realloc(list->data,
								  list->allocated * sizeof(cfglib_data_t *));
	}

	list->data[list->items++] = data;
}

static void merge_lists(cfglib_list_t *dest, cfglib_list_t *src)
{
	int i;

	for(i = 0; i < src->items; i++)
		add_to_list(dest, src->data[i]);
	free(src->data);
	free(src);
}

static cfglib_value_t *parse_value(FILE *fp, cfglib_option_t *opt, char *parameter)
{
	cfglib_value_t *value = alloc_value();
	char *p;
	cfglib_list_t *list;
	char *e;

	value->opt = opt;
	
	if(!cfgftest(opt->flags, CFGF_LIST)) {
		value->data = parse_data(fp, opt, parameter);
		return value;
	}

	/* the rest of this routine handles lists */
	
	value->data = (cfglib_data_t *)malloc(sizeof(cfglib_data_t));

	list = &value->data->val_list;

	list->items = 0;
	list->allocated = 0;
	list->data = 0;

	p = parameter;
	if(*p == '{')
		++p;

	if(*p == 0)
		p = cfglib_nextstr(fp);
	
	do {
		if(strcmp(p, "}") == 0)
			break;

		e = strrchr(p, 0);
		if(e[-1] == '}') {
			/* last value, strip closing brace */
			e[-1] = 0;
			add_to_list(list, parse_data(fp, opt, p));
			break;
		}

		cfglib_trim_end(p, ",", 0);

		add_to_list(list, parse_data(fp, opt, p));

		p = cfglib_nextstr(fp);
	} while(!feof(fp));
	return value;
}

static void add_to_values(cfglib_value_list_t **values, cfglib_value_t *value)
{
	if(!*values) {
		*values = (cfglib_value_list_t *)malloc(sizeof(cfglib_value_list_t));
		memset(*values, 0, sizeof(cfglib_value_list_t));
	}

	/* YES! I love pointers! :-) */

	if((*values)->allocated <= (*values)->items) {
		(*values)->allocated += 10;
		(*values)->values = (cfglib_value_t **)realloc((*values)->values,
						  (*values)->allocated * sizeof(cfglib_value_t *));
	}

	(*values)->values[(*values)->items++] = value;
}

static void indent(int level)
{
	int i;
	for(i = 0; i < level; i++)
		fprintf(stderr, "  ");
}

static int check_endblock(char *str)
{
	char *e = str ? strrchr(str, 0) : 0;
	if(e && e[-1] == '}') { /* FIXME: \-quoted !? */
		e[-1] = 0;
		return 1;
	}
	return 0;
}

int cfglib_parse(FILE *fp, cfglib_option_t *opts,
				 cfglib_value_list_t **values, int level)
{
	while(!feof(fp)) {
		char *option;
		cfglib_option_t *opt;
		char *parameter;
		char *eq;
		int end_block = 0;

		if((option = cfglib_nextstr(fp)) == 0)
			break;

		if(strcmp(option, "}") == 0) {
			if(level == 0)
				fprintf(stderr, "Doh!\n");
			break;
		}

		end_block = check_endblock(option);
		
		if(strcasecmp(option, "include") == 0) {
			char *fname = cfglib_nextstr(fp);
			/* error check */
			cfglib_load(fname, opts, values);
		}
		
		parameter = 0;
		
		if((eq = strchr(option, '=')) != 0) {
			*eq = 0;
			if(eq[1] != 0)
				parameter = eq + 1;
		}

		opt = find_opt(option, opts);
		if(opt == 0)
			fprintf(stderr, "unknown option '%s'\n", option);
		else {
			cfglib_value_t *value;
			cfglib_value_t *old_value;
			int skip = 0;
			int merge = 0;

			if(opt->name == 0)
				option = strdup(option);

			if(opt->type != ct_toggle) {
				if(!parameter)
					parameter = cfglib_nextstr(fp);
				/* FIXME: error check */
				if(strcmp(parameter, "=") == 0)
					parameter = cfglib_nextstr(fp);
				else if(parameter[0] == '=')
					parameter++;
			}

			end_block = check_endblock(parameter);

			if(!opt->subopts)
				value = parse_value(fp, opt, parameter);
			else {
				value = alloc_value();
				value->opt = opt;
			}

			value->name = opt->name ? strdup(opt->name) : option;

			old_value = values ? find_value(value->name, *values) : 0;
			/* this option has already been specified */
			if(old_value) {
				if(cfgftest(opt->flags, CFGF_LIST_MERGE_MULTIPLES)
						&& cfgftest(opt->flags, CFGF_LIST))
					merge = 1;
				else if(cfgftest(opt->flags, CFGF_ALLOW_MULTIPLES))
					;
				else {
					indent(level);
					fprintf(stderr, "%s: multiply defined\n", value->name);
					skip = 1;
				}
			}

			if(opt->subopts) {
				if(strcmp(parameter, "{") != 0)
					fprintf(stderr, "syntax error\n");
				cfglib_setparent(opt->subopts, opt);
				value->data = (cfglib_data_t *)malloc(sizeof(cfglib_data_t));
				cfglib_parse(fp, opt->subopts, &value->data->val_sub,
							 level + 1);
			}

			if(opt->callback && !skip)
				opt->callback(value);

			/* do we wan't to save the values? */
			if(!skip && values) {
				if(merge) {
					merge_lists(&old_value->data->val_list,
								&value->data->val_list);
				} else {
					add_to_values(values, value);
				}
			} else /* nope, throw it away */
				cfglib_free_value(value);
		}
		if(end_block)
			break;
	}
	
	return 0;
}

int cfglib_load(const char *filename, cfglib_option_t *opts,
				cfglib_value_list_t **values)
{
	FILE *fp;
	int ret;

	if((fp = fopen(filename, "r")) == 0) {
		perror(filename);
		return -1;
	}
	ret = cfglib_parse(fp, opts, values, 0);
	fclose(fp);
	return ret;
}
