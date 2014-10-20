/***************************************************************************
 *   Copyright (C) 2009 - 2010 by Simon Qian <SimonQian@SimonQian.com>     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#ifndef __VSFSHELL_H_INCLUDED__
#define __VSFSHELL_H_INCLUDED__

#include "dal/stream/stream.h"
#include "framework/vsfsm/vsfsm.h"

#define VSFSHELL_HANDLER(name, thread)		{(name), (thread), NULL}
#define VSFSHELL_HANDLER_NONE				VSFSHELL_HANDLER(NULL, NULL)
struct vsfshell_handler_t
{
	const char * const name;
	vsf_err_t (*thread)(struct vsfsm_pt_t *pt, vsfsm_evt_t evt);
	struct vsfshell_handler_t *next;
};

#define VSFSHELL_LINEEND					"\n\r"
#define VSFSHELL_PROMPT						">>>"

struct vsfshell_t
{
	struct vsf_stream_t *stream_tx;
	struct vsf_stream_t *stream_rx;
	
	struct vsfsm_t sm;
	
	// private
	char cmd_buff[256];
	struct vsf_transaction_buffer_t tbuffer;
	struct vsfsm_t *frontend_handler;
	struct vsfshell_handler_t *handlers;
};

struct vsfshell_handler_param_t
{
	uint8_t argc;
	char *argv[16];
	
	// private
	struct vsfshell_t *shell;
	void *priv;
	struct vsfsm_t sm;
	struct vsfsm_pt_t pt;
};

vsf_err_t vsfshell_init(struct vsfshell_t *shell);
void vsfshell_register_handlers(struct vsfshell_t *shell,
										struct vsfshell_handler_t *handlers);

#endif	// __VSFSHELL_H_INCLUDED__
