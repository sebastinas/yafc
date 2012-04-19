/*
 * transfer.h --
 *
 * Yet Another FTP Client
 * Copyright (C) 1998-2001, Martin Hedenfalk <mhe@stacken.kth.se>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. See COPYING for more details.
 */

#ifndef _transfer_h_included
#define _transfer_h_included

void transfer(transfer_info *ti);
int transfer_init_nohup(const char *str);
void transfer_begin_nohup(int argc, char **argv);
void transfer_end_nohup(void);
void transfer_nextfile(list *gl, listitem **li, bool removeitem);
void transfer_mail_msg(const char *fmt, ...);

void add_ascii_transfer_masks(const char *str);
bool ascii_transfer(const char *mask);
bool transfer_first(const char *mask);
bool ignore(const char *mask);

extern const char *status;
extern const char *finished_status;

#endif
