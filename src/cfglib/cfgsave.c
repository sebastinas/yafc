#include <stdio.h>
#include "cfglib.h"

static int cfglib_save_internal(FILE *fp, cfglib_value_list_t *values,
								int indent);

static void indent_line(FILE *fp, int level)
{
	int i;
	for(i = 0; i < level; i++)
		fprintf(fp, "  ");
}

static void save_comment(FILE *fp, const char *comment, int indent)
{
	const char *e;

	indent_line(fp, indent);

	fprintf(fp, "# ");

	for(e = comment; e && *e; e++) {
		if(*e == '\n') {
			fputc('\n', fp);
			indent_line(fp, indent);
			fprintf(fp, "# ");
		} else
			fputc(*e, fp);
	}
	fputc('\n', fp);
}

static void save_data(FILE *fp, cfglib_data_t *data, cfglib_option_t *opt)
{
	switch(opt->type) {
	case ct_string:
		fprintf(fp, "%s", data->val_string);
		break;
	case ct_int:
		fprintf(fp, "%d", data->val_int);
		break;
	case ct_bool:
		fprintf(fp, "%s", data->val_bool ? "true" : "false");
		break;
	case ct_toggle:
		if(data->val_toggle)
			fprintf(fp, "%s",  opt->name);
		break;
	case ct_sub:
		break;
	}
}

static void save_value(FILE *fp, cfglib_value_t *value, int indent)
{
	cfglib_option_t *opt = value->opt;

	if(opt->comment && opt->comment[0]) {
		fputc('\n', fp);
		save_comment(fp, opt->comment, indent);
	}

	indent_line(fp, indent);

	if(value->opt->type != ct_toggle)
		fprintf(fp, "%s = ", value->name);

	if(cfgftest(opt->flags, CFGF_LIST)) {
		int i;
		cfglib_list_t *list = &value->data->val_list;
		fputc('{', fp);
		for(i = 0; i < list->items; i++) {
			save_data(fp, list->data[i], opt);
			if(i < list->items - 1)
				fprintf(fp, ", ");
		}
		fputc('}', fp);
	} else if(opt->subopts) {
		fprintf(fp, "{\n");
		cfglib_save_internal(fp, value->data->val_sub, indent+1);
		indent_line(fp, indent);
		fputc('}', fp);
	} else
		save_data(fp, value->data, opt);
}

static int cfglib_save_internal(FILE *fp, cfglib_value_list_t *values,
								int indent)
{
	int i;

	if(!values)
		return 0;

	for(i = 0; i < values->items; i++) {
		save_value(fp, values->values[i], indent);
		fputc('\n', fp);
	}
	return 0;
}

int cfglib_save(const char *filename, cfglib_value_list_t *values)
{
	FILE *fp;
	int ret;

	if(!values) {
		fprintf(stderr, "cfglib_save: nothing to save\n");
		return -1;
	}

	if((fp = fopen(filename, "w")) == 0) {
		perror(filename);
		return -1;
	}
	ret = cfglib_save_internal(fp, values, 0);
	fprintf(fp, "# end of configuration file\n");
	fclose(fp);
	return ret;
}
