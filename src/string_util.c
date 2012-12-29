/*****************************************************************************
 *
 * string.c
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Copyright (C) 1999-2012 William Weston <whw@linuxmail.org>
 *
 * PHASEX is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * PHASEX is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with PHASEX.  If not, see <http://www.gnu.org/licenses/>.
 *
 *****************************************************************************/
#include <unistd.h>
#include <string.h>


/*****************************************************************************
 * get_next_token()
 *
 * Use in a while loop to split a patch definition line into its tokens.
 * Whitespace always delimits a token.  The special tokens '{', '}', '=', and
 * ';' are always tokenized, regardless of leading or trailing whitespace.
 *****************************************************************************/
char *
get_next_token(char *inbuf)
{
	unsigned int    len;
	int             in_quote        = 0;
	static int      eob             = 1;
	char            *token_begin;
	static char     *t_index          = NULL;
	static char     *last_inbuf     = NULL;
	static char     token_buf[256];

	/* keep us out of trouble */
	if ((inbuf == NULL) && (last_inbuf == NULL)) {
		return NULL;
	}

	/* was end of buffer set last time? */
	if (eob) {
		eob = 0;
		t_index = inbuf;
	}

	/* keep us out of more trouble */
	if ((t_index == NULL) || (*t_index == '\0') ||
	    (*t_index == '#') || (*t_index == '\n')) {
		eob = 1;
		return NULL;
	}

	/* skip past whitespace */
	while ((*t_index == ' ') || (*t_index == '\t')) {
		t_index++;
	}

	/* check for quoted token */
	if (*t_index == '"') {
		in_quote = 1;
		t_index++;
	}

	/* we're at the start of the current token */
	token_begin = t_index;

	/* go just past the last character of the token */
	if (in_quote) {
		while (*t_index != '"') {
			t_index++;
		}
		//t_index++;
	}
	else if ((*t_index == '{') || (*t_index == '}') ||
	         (*t_index == ';') || (*t_index == '=') || (*t_index == ',')) {
		t_index++;
	}
	else {
		while ((*t_index != ' ')  && (*t_index != '\t') &&
		       (*t_index != '{')  && (*t_index != '}')  &&
		       (*t_index != ';')  && (*t_index != '=')  &&
		       (*t_index != '\0') && (*t_index != '\n') &&
		       (*t_index != '#')  && (*t_index != ',')) {
			t_index++;
		}
	}

	/* check for end of buffer (null, newline, or comment delim) */
	if ((t_index == token_begin) &&
	    ((*t_index == '\0') || (*t_index == '\n') || (*t_index == '#'))) {
		eob = 1;
		return NULL;
	}

	/* copy the token to our static buffer and terminate */
	len = (long unsigned int)(t_index - token_begin) % sizeof(token_buf);
	strncpy(token_buf, token_begin, len);
	token_buf[len] = '\0';

	/* skip past quote */
	if (in_quote) {
		t_index++;
	}

	/* skip past whitespace */
	while ((*t_index == ' ') || (*t_index == '\t')) {
		t_index++;
	}

	/* return the address to the token buffer */
	return token_buf;
}
