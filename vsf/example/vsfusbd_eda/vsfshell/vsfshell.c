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

#include "app_type.h"
#include "compiler.h"

#include "dal/stream/stream.h"
#include "framework/vsfsm/vsfsm.h"

#include "vsfshell.h"

enum vsfshell_EVT_t
{
	VSFSHELL_EVT_STREAMRX_ONIN = VSFSM_EVT_USER_LOCAL + 0,
	VSFSHELL_EVT_STREAMTX_ONOUT = VSFSM_EVT_USER_LOCAL + 1,
//	VSFSHELL_EVT_ = VSFSM_EVT_USER_LOCAL_INSTANT + 2,
};

static void vsfshell_streamrx_callback_on_in_int(void *p)
{
	struct vsfshell_t *shell = (struct vsfshell_t *)p;
	
	vsfsm_post_evt_pending(&shell->sm, VSFSHELL_EVT_STREAMRX_ONIN);
}

static void vsfshell_streamtx_callback_on_out_int(void *p)
{
	struct vsfshell_t *shell = (struct vsfshell_t *)p;
	
	vsfsm_post_evt_pending(&shell->sm, VSFSHELL_EVT_STREAMTX_ONOUT);
}

static struct vsfsm_state_t *
vsfshell_evt_handler(struct vsfsm_t *sm, vsfsm_evt_t evt)
{
	struct vsfshell_t *shell = (struct vsfshell_t *)sm->user_data;
	struct vsf_buffer_t buffer;
	uint8_t buff[64];
	
	switch (evt)
	{
	case VSFSM_EVT_INIT:
		shell->stream_rx->callback_rx.param = shell;
		shell->stream_rx->callback_rx.on_in_int =
							vsfshell_streamrx_callback_on_in_int;
		shell->stream_tx->callback_tx.param = shell;
		shell->stream_tx->callback_tx.on_out_int =
							vsfshell_streamtx_callback_on_out_int;
		break;
	case VSFSHELL_EVT_STREAMRX_ONIN:
		buffer.buffer = buff;
		buffer.size = sizeof(buff);
		buffer.size = stream_rx(shell->stream_rx, &buffer);
		stream_tx(shell->stream_tx, &buffer);
		break;
	case VSFSHELL_EVT_STREAMTX_ONOUT:
		break;
	}
	
	return NULL;
}

vsf_err_t vsfshell_init(struct vsfshell_t *shell)
{
	// state machine init
	shell->sm.init_state.evt_handler = vsfshell_evt_handler;
	shell->sm.user_data = (void*)shell;
	return vsfsm_init(&shell->sm, true);
}
