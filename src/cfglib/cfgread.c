#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "cfglib.h"

/* expands ${ENVAR:-default}
 * returns new allocated string (should be free()'d)
 */
char *cfglib_env_expand(char *parameter)
{
	char *x = strstr(parameter, "${");
	if(x) {
		char *exp;
		char *e;
		char *app;

		*x = 0;
		x += 2;

		e = strchr(x, '}');
		if(e) {
			*e = 0;
			app = e+1;
			e = strchr(x, ':');
			if(e)
				*e = 0;

			exp = getenv(x);

			if(exp == 0) {
				if(e && e[1] == '-')
					exp = e + 2;
				else
					exp = "";
			}
				
			e = (char *)malloc(strlen(parameter)+strlen(exp)+strlen(app)+1);
			strcpy(e, parameter);
			strcat(e, exp);
			strcat(e, app);
			return e;
		}
	}
	return strdup(parameter);
}


/* appends APPSTR to STR (which has allocated size LEN)
 * (re-)allocates STR if needed
 */
static void string_append(char **str, size_t *len, char *appstr)
{
	int curlen = *str ? strlen(*str) : 0;
	size_t required_len = curlen + strlen(appstr) + 1;
	if(*len <= required_len) {
		*str = realloc(*str, required_len);
		*len = required_len;
	}

	if(curlen == 0)
		strcpy(*str, appstr);
	else
		strcat(*str, appstr);
}

static void skip_line(FILE *fp)
{
	while(1) {
		char c = fgetc(fp);

		if(c == EOF || c == '\n')
			break;
	}
}

static void skip_space(FILE *fp)
{
	while(1) {
		char c = fgetc(fp);

		if(c == EOF)
			break;
		if(!isspace(c)) {
			ungetc(c, fp);
			break;
		}
	}
}

/* returns the next space-separated string from FP
 * quote character are recognized (and included in the string)
 * comments (beginning with '#') are skipped, as are blank lines
 *
 * the string is allocated in static memory and overwritten with each call
 */
char *cfglib_nextstr(FILE *fp)
{
	static char tmp[257];
	int i = 0;
	char inquote = 0;

	skip_space(fp);

	while(i < 257) {
		char c = fgetc(fp);
		if(c == EOF)
			break;

		/* skip comments */
		if(c == '#') {
			skip_line(fp);
			skip_space(fp);
			if(i)
				break;
			else continue;
		}

		if(!inquote && isspace(c)) {
			ungetc(c, fp);
			break;
		}

		if(c == '\\') {
			tmp[i++] = '\\';
			c = fgetc(fp);;
			if(c == EOF)
				break;
			tmp[i++] = c;
			continue;
		}

		if(c == '\'') {
			if(inquote == '\'')
				inquote = 0;
			else
				inquote = '\'';
		} else if(c == '\"') {
			if(inquote == '\"')
				inquote = 0;
			else
				inquote = '\"';
		}

		tmp[i++] = c;
	}
	tmp[i] = 0;

	if(i == 0 && feof(fp))
		return 0;
	return tmp;
}


char *cfglib_read_string(FILE *fp, char *parameter)
{
	if(strncmp(parameter, "<<", 2) == 0) {
		/* here-document */
		char tmp[257];
		char *here_string = 0;
		size_t here_len = 0;

		char *delimiter;
		if(parameter[2] == 0)
			delimiter = strdup(cfglib_nextstr(fp));
		else
			delimiter = strdup(parameter+2);

		skip_line(fp);

		while(fgets(tmp, 256, fp)) {
			if(strncmp(tmp, delimiter, strlen(delimiter)) == 0)
				break;
			string_append(&here_string, &here_len, tmp);
		}
		free(delimiter);

		cfglib_trim_end(here_string, "\n", 0);

		if(feof(fp))
			fprintf(stderr, "Warning: HERE-document not terminated!\n");

		return here_string;
	}
	return cfglib_env_expand(parameter);
}
