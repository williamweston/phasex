/*****************************************************************************
 *
 * param_parse.h
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Copyright (C) 1999-2013 William Weston <whw@linuxmail.org>
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
#ifndef _PHASEX_PARAM_PARSE_H_
#define _PHASEX_PARAM_PARSE_H_


sample_t get_rate_val(int ctlr);
int get_rate_ctlr(char *token, char *UNUSED(filename), int UNUSED(line));
int get_wave(char *token, char *filename, int line);
int get_polarity(char *token, char *filename, int line);
int get_ctlr(char *token, char *UNUSED(filename), int UNUSED(line));
int get_boolean(char *token, char *UNUSED(filename), int UNUSED(line));
int get_freq_base(char *token, char *UNUSED(filename), int UNUSED(line));


#endif /* _PHASEX_PARAM_PARSE_H_ */
