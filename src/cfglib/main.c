#include <stdio.h>
#include "cfglib.h"

void cb_bool(cfglib_value_t *value)
{
	fprintf(stderr, "got bool %s (#%d), value: %s\n",
		   value->name, (int)value->opt->data,
			value->data->val_bool ? "on" : "off");
}

void cb_string(cfglib_value_t *value)
{
	if(value->opt->flags & CFGF_LIST) {
		int i;
		fprintf(stderr, "got string list %s, value:\n", value->name);
		for(i=0; i < value->data->val_list.items; i++)
			fprintf(stderr, "  '%s'\n",
					value->data->val_list.data[i]->val_string);
	} else {
		fprintf(stderr, "got string %s (#%d), value: '%s'\n",
				value->name, (int)value->opt->data,
				value->data->val_string);
	}
}

void cb_int(cfglib_value_t *value)
{
	fprintf(stderr, "got int %s (#%d), value: %d\n",
			value->name, (int)value->opt->data, value->data->val_int);
}

void cb_machine(cfglib_value_t *value)
{
	static int lastmachine = -1;

	++lastmachine;
	fprintf(stderr, "  allocating a new machine #%d\n", lastmachine);
	(int)value->opt->data = lastmachine;
}

void cb_machine_define(cfglib_value_t *value)
{
	fprintf(stderr, "      machine #%d: ", value->opt->parent->data);
	switch(value->opt->data) {
	case 0: /* host */
		fprintf(stderr, "host: %s\n", value->data->val_string);
		break;
	case 1: /* login */
		fprintf(stderr, "login: %s\n", value->data->val_string);
		break;
	case 2: /* alias */
		fprintf(stderr, "alias: %s\n", value->data->val_string);
		break;
	case 3: /* password */
		fprintf(stderr, "password: %s\n", value->data->val_string);
		break;
	case 4: /* passive */
		fprintf(stderr, "passive: %s\n", value->data->val_bool ? "on" : "off");
		break;
	case 5: /* cwd */
		fprintf(stderr, "cwd: %s\n", value->data->val_string);
		break;
	case 6: /* anonymous */
		fprintf(stderr, "anonymous: %s\n", value->data->val_toggle ? "yes" : "no");
		break;
	case 7: /* prot */
		fprintf(stderr, "prot: %s\n", value->data->val_string);
		break;
	default:
		fprintf(stderr, "got something else\n");
	}
}

void cb_alias_define(cfglib_value_t *value)
{
	fprintf(stderr, "    got alias '%s' = '%s'\n",
			value->name, value->data->val_string);
}

void cb_alias(cfglib_value_t *value)
{
	fprintf(stderr, "  initializing aliases...\n");
}

void cb_proxy(cfglib_value_t *value)
{
	fprintf(stderr, "  initializing proxy...\n");
}

void cb_bookmarks(cfglib_value_t *value)
{
	fprintf(stderr, "  initializing bookmarks...\n");
}

void cb_bm(cfglib_value_t *value)
{
	static int lastmachine = -1;

	++lastmachine;
	fprintf(stderr, "    allocating a new bookmark %s (#%d)\n",
			value->name, lastmachine);
	(int)value->opt->data = lastmachine;
}

int main(int argc, char **argv)
{
	cfglib_option_t machine_opts[] = {
		CFG_STR("host", 0, cb_machine_define, 0, 0),
		CFG_STR("login", 0, cb_machine_define, 1, 0),
		CFG_STR("alias",0, cb_machine_define, 2, 0),
		CFG_STR("password",0,cb_machine_define, 3, 0),
		CFG_BOOL("passive", 0, cb_machine_define, 4, 0),
		CFG_STR("cwd", 0, cb_machine_define, 5, 0),
		CFG_TOGGLE("anonymous", 0, cb_machine_define, 6, 0),
		CFG_STR("prot", 0, cb_machine_define, 7, 0),
		CFG_ENDOPT
	};

	cfglib_option_t alias_opts[] = {
		CFG_STR(0 /* all names valid */, 0, cb_alias_define, 0, 0),
		CFG_ENDOPT
	};

	cfglib_option_t proxy_opts[] = {
		CFG_INT("type", 0, 0, 0, 0),
		CFG_STR("host", 0, cb_string, 17, 0),
		CFG_STR("exclude", CFGF_LIST, cb_string, 0, 0),
		CFG_ENDOPT
	};

	cfglib_option_t bookmark_opts[] = {
		CFG_SUB(0 /* all names valid */, 0, cb_bm, 0, machine_opts, ""),
		CFG_ENDOPT
	};

	cfglib_option_t opts[] = {
		/* boolean boolean */
		CFG_BOOL("autologin", 0, cb_bool, 0,
				"attempt to login automagically, using the autologin " \
				"entries below"),
		CFG_BOOL("quit_on_eof", 0, cb_bool, 1, "quit program if " \
				"received EOF (C-d)"),
		CFG_BOOL("use_passive_mode", 0, cb_bool, 2,
				"use passive mode connections\n" \
				"if false, use sendport mode\n" \
				"if you get the message 'Host doesn't support passive " \
				"mode', set to false"),
		CFG_BOOL("read_netrc", 0, cb_bool, 3, "read netrc"),
		CFG_BOOL("verbose", 0, cb_bool, 4, ""),
		CFG_BOOL("debug", 0, cb_bool, 5, ""),
		CFG_BOOL("trace", 0, cb_bool, 6, ""),
		CFG_BOOL("inhibit_startup_syst", 0, cb_bool, 7, ""),
		CFG_BOOL("use_env_string", 0, cb_bool, 8, ""),
		CFG_BOOL("remote_completion", 0, cb_bool, 9, ""),
		CFG_BOOL("auto_bookmark", 0, cb_bool, 10, ""),
		CFG_BOOL("auto_bookmark_save_passwd", 0, cb_bool, 11, ""),
		CFG_BOOL("auto_bookmark_silent", 0, cb_bool, 12, ""),
		CFG_BOOL("beep_after_long_command", 0, cb_bool, 13, ""),
		CFG_BOOL("use_history", 0, cb_bool, 14, ""),
		CFG_BOOL("load_taglist", 0, cb_bool, 15, ""),
		CFG_BOOL("tilde", 0, cb_bool, 16, ""),

		/* integer options */
		CFG_INT("long_command_time", 0, cb_int, 0, ""),
		CFG_INT("history_max", 0, cb_int, 1, ""),
		CFG_INT("command_timeout", 0, cb_int, 2, ""),
		CFG_INT("connection_timeout", 0, cb_int, 3, ""),
		CFG_INT("connect_attempts", 0, cb_int, 4, ""),
		CFG_INT("connect_wait_time", 0, cb_int, 5, ""),
		CFG_INT("proxy_type", 0, cb_int, 6, ""),

		/* string options */
		CFG_STR("default_type", 0, cb_string, 0, ""),
		CFG_STR("default_mechanism", CFGF_LIST|CFGF_LIST_MERGE_MULTIPLES,
				cb_string, 1, ""),
		CFG_STR("ascii_transfer_mask", CFGF_LIST|CFGF_LIST_MERGE_MULTIPLES,
				cb_string, 2, ""),
		CFG_STR("prompt1", 0, cb_string, 3, ""),
		CFG_STR("prompt2", 0, cb_string, 4, ""),
		CFG_STR("prompt3", 0, cb_string, 5, ""),
		CFG_STR("xterm_title1", 0, cb_string, 6, ""),
		CFG_STR("xterm_title2", 0, cb_string, 7, ""),
		CFG_STR("xterm_title3", 0, cb_string, 8, ""),
		CFG_STR("transfer_begin_string", 0, cb_string, 9, ""),
		CFG_STR("transfer_string", 0, cb_string, 10, ""),
		CFG_STR("transfer_end_string", 0, cb_string, 11, ""),
		CFG_STR("xterm_title_terms", CFGF_LIST|CFGF_LIST_MERGE_MULTIPLES,
				cb_string, 11, ""),

		/* suboptions */
		CFG_SUB("machine", CFGF_ALLOW_MULTIPLES, cb_machine, 0, machine_opts, "bookmarks"),
		CFG_SUB("alias", 0, cb_alias, 0, alias_opts,
				   "aliases (on the form [alias name value])\n" \
				   "can't make an alias of another alias"),
		CFG_SUB("proxy", 0, cb_proxy, 0, proxy_opts, "proxy settings"),

		CFG_SUB("bookmarks", CFGF_ALLOW_MULTIPLES, cb_bookmarks, 0, bookmark_opts, "bookmarks"),
		
		CFG_ENDOPT
	};

	cfglib_value_list_t *values = 0;

	if(argc > 1) {
		cfglib_value_list_t *bookmarks;

		printf("parsing %s\n", argv[1]);
		cfglib_load(argv[1], opts, &values);
		printf("value of prompt1: %s\n", cfglib_getstring("prompt1", values));

		bookmarks = cfglib_getsub("bookmarks", values);
		printf("bookmarks->default->login == %s\n",
			   cfglib_getstring("login", cfglib_getsub("default", bookmarks)));

		printf("\nsaving parsed config file to foo.conf...\n");
		cfglib_save("foo.conf", values);
	} else
		printf("syntax terror\n");
	return 0;

}
