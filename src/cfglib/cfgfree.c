#include <stdlib.h>
#include "cfglib.h"

static void cfglib_free_data_internal(cfglib_data_t *data,
									  const cfglib_option_t *opt)
{
	if(data) {
		if(opt->type == ct_string)
			free(data->val_string);
	}
}

static void cfglib_free_list(cfglib_list_t *list, const cfglib_option_t *opt)
{
	if(list) {
		int i;

		for(i = 0; i < list->items; i++)
			cfglib_free_data_internal(list->data[i], opt);
		free(list->data);
	}
}

static void cfglib_free_data(cfglib_data_t *data, const cfglib_option_t *opt)
{
	if(data) {
		if(opt->type == ct_sub)
			cfglib_free_values(data->val_sub);
		else if(cfgftest(opt->flags, CFGF_LIST))
			cfglib_free_list(&data->val_list, opt);
		else
			cfglib_free_data_internal(data, opt);
		free(data);
	}
}

void cfglib_free_value(cfglib_value_t *value)
{
	if(value) {
		free(value->name);
		value->name = 0;
		cfglib_free_data(value->data, value->opt);
		value->data = 0;
		free(value);
	}
}

void cfglib_free_values(cfglib_value_list_t *values)
{
	int i;

	for(i = 0; i < values->items; i++)
		cfglib_free_value(values->values[i]);
	free(values);
}
