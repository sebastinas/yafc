/* $Id: text2c.c,v 1.3 2001/05/12 18:44:37 mhe Exp $
 *
 * text2c.c -- convert a text file to a C string
 *
 * Yet Another FTP Client
 * Copyright (C) 1998-2001, Martin Hedenfalk <mhe@stacken.kth.se>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. See COPYING for more details.
 */

#include <stdio.h>

int main(int argc, char **argv)
{
	FILE *in, *out;
	char tmp[1024], *var;

	if(argc <= 1 || strcmp(argv[1], "-") == 0)
		in = stdin;
	else {
		in = fopen(argv[1], "r");
		if(!in) {
			perror(argv[1]);
			return 1;
		}
	}

	if(argc <= 2 || strcmp(argv[2], "-") == 0)
		out = stdout;
	else {
		out = fopen(argv[2], "w");
		if(!out) {
			perror(argv[2]);
			return 2;
		}
	}

	if(argc > 3)
		var = argv[3];
	else
		var = "text2c";


	fprintf(out, "const char *%s = \"\"\n", var);

	while(fgets(tmp, 1024, in) != 0) {
		char *e;
		fputc('\"', out);
		for(e=tmp; *e && *e!='\r' && *e!='\n'; e++) {
			if(*e == '\"' || *e == '\\')
				fputc('\\', out);
			fputc(*e, out);
		}
		fprintf(out, "\\n\"\n");
	}
	fprintf(out, ";\n");
	fclose(out);
	fclose(in);
	return 0;
}
