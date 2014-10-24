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

#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>

#include "compiler.h"
#include "app_type.h"

#include "framework/vsftimer/vsftimer.h"
#include "vsfshell.h"

#include <stdlib.h>
#define MALLOC			malloc
#define FREE			free
#define STRDUP			strdup

// handlers
static vsf_err_t
vsfshell_echo_handler(struct vsfsm_pt_t *pt, vsfsm_evt_t evt);
static vsf_err_t
vsfshell_delayms_handler(struct vsfsm_pt_t *pt, vsfsm_evt_t evt);
static struct vsfshell_handler_t vsfshell_handlers[] =
{
	VSFSHELL_HANDLER("echo", vsfshell_echo_handler),
	VSFSHELL_HANDLER("delayms", vsfshell_delayms_handler),
	VSFSHELL_HANDLER_NONE
};

enum vsfshell_EVT_t
{
	VSFSHELL_EVT_STREAMRX_ONIN = VSFSM_EVT_USER_LOCAL + 0,
	VSFSHELL_EVT_STREAMTX_ONOUT = VSFSM_EVT_USER_LOCAL + 1,
	VSFSHELL_EVT_STREAMRX_ONCONN = VSFSM_EVT_USER_LOCAL + 2,
	VSFSHELL_EVT_STREAMTX_ONCONN = VSFSM_EVT_USER_LOCAL + 3,
	
	VSFSHELL_EVT_FRONT_HANDLER_EXIT = VSFSM_EVT_USER_LOCAL_INSTANT + 0,
	VSFSHELL_EVT_OUTPUT_CRIT_AVAIL = VSFSM_EVT_USER_LOCAL_INSTANT + 1,
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

static void vsfshell_streamrx_callback_on_txconn(void *p)
{
	struct vsfshell_t *shell = (struct vsfshell_t *)p;
	
	vsfsm_post_evt_pending(&shell->sm, VSFSHELL_EVT_STREAMRX_ONCONN);
}

static void vsfshell_streamtx_callback_on_rxconn(void *p)
{
	struct vsfshell_t *shell = (struct vsfshell_t *)p;
	
	vsfsm_post_evt_pending(&shell->sm, VSFSHELL_EVT_STREAMTX_ONCONN);
}

// vsfshell_output_thread is used to process the events
// 		from the receiver of the stream_tx
vsf_err_t vsfshell_output_thread(struct vsfsm_pt_t *pt, vsfsm_evt_t evt,
									const char *format, ...)
{
	struct vsfshell_t *shell = (struct vsfshell_t *)pt->user_data;
	uint32_t str_len, size_avail;
	struct vsf_buffer_t buffer;
	va_list ap;
	char *printf_buff;
	uint32_t printf_size;
	
	vsfsm_pt_begin(pt);
	// get lock here
	if (vsfsm_crit_enter(pt->sm, &shell->output_crit))
	{
		vsfsm_pt_wfe(pt, VSFSHELL_EVT_OUTPUT_CRIT_AVAIL);
	}
	shell->output_sm = pt->sm;
	
	if (shell->output_interrupted && (pt->sm == &shell->sm))
	{
		// is vsfshell_input_thread is interrupted
		// 		and current pt is vsfshell_input_thread
		// then output the VSFSHELL_PROMPT and the original commandline
		shell->output_interrupted = false;
		
		size_avail = stream_get_free_size(shell->stream_tx);
		while (size_avail < strlen(VSFSHELL_PROMPT))
		{
			vsfsm_pt_wfe(pt, VSFSHELL_EVT_STREAMTX_ONOUT);
			size_avail = stream_get_free_size(shell->stream_tx);
		}
		buffer.buffer = (uint8_t *)VSFSHELL_PROMPT;
		buffer.size = strlen(VSFSHELL_PROMPT);
		stream_tx(shell->stream_tx, &buffer);
		
		shell->tbuffer.buffer.buffer[shell->tbuffer.position] = '\0';
		shell->printf_pos = (char *)shell->tbuffer.buffer.buffer;
		str_len = strlen(shell->printf_pos);
		while (str_len > 0)
		{
			size_avail = stream_get_free_size(shell->stream_tx);
			if (!size_avail)
			{
				vsfsm_pt_wfe(pt, VSFSHELL_EVT_STREAMTX_ONOUT);
				size_avail = stream_get_free_size(shell->stream_tx);
			}
			
			if (size_avail)
			{
				buffer.buffer = (uint8_t *)shell->printf_pos;
				buffer.size = min(str_len, size_avail);
				buffer.size = stream_tx(shell->stream_tx, &buffer);
				shell->printf_pos += buffer.size;
				str_len = strlen(shell->printf_pos);
			}
		}
	}
	
	va_start(ap, format);
	if (pt->sm != shell->frontend_sm)
	{
		// if current pt is not frontend, then add a new line
		if (shell->frontend_sm == &shell->sm)
		{
			// if current frontend pt is vsfshell_input_thread
			// 		then set output_interrupted, so that current command line
			// 		input will be recovered later
			shell->output_interrupted = true;
		}
		strcpy(shell->printf_buff, VSFSHELL_LINEEND);
		printf_size = sizeof(shell->printf_buff) - strlen(VSFSHELL_LINEEND);
		printf_buff = shell->printf_buff + strlen(VSFSHELL_LINEEND);
	}
	else
	{
		printf_size = sizeof(shell->printf_buff);
		printf_buff = shell->printf_buff;
	}
	str_len = vsnprintf(printf_buff, printf_size, format, ap);
	va_end(ap);
	shell->printf_pos = shell->printf_buff;
	
	while (str_len > 0)
	{
		size_avail = stream_get_free_size(shell->stream_tx);
		if (!size_avail)
		{
			vsfsm_pt_wfe(pt, VSFSHELL_EVT_STREAMTX_ONOUT);
			size_avail = stream_get_free_size(shell->stream_tx);
		}
		
		if (size_avail)
		{
			buffer.buffer = (uint8_t *)shell->printf_pos;
			buffer.size = min(str_len, size_avail);
			buffer.size = stream_tx(shell->stream_tx, &buffer);
			shell->printf_pos += buffer.size;
			str_len = strlen(shell->printf_pos);
		}
	}
	shell->output_sm = NULL;
	vsfsm_crit_leave(pt->sm, &shell->output_crit);
	vsfsm_pt_end(pt);
	
	return VSFERR_NONE;
}

static vsf_err_t
vsfshell_parse_cmd(char *cmd, struct vsfshell_handler_param_t *param)
{
	uint8_t cmd_len, argv_num;
	uint16_t i;
	char *argv_tmp;
	
	memset(param->argv, 0, sizeof(param->argv));
	cmd_len = strlen(cmd);
	argv_num = 0;
	i = 0;
	while (i < cmd_len)
	{
		while (isspace((int)cmd[i]))
		{
			i++;
		}
		if ('\0' == cmd[i])
		{
			break;
		}
		
		if (('\'' == cmd[i]) || ('"' == cmd[i]))
		{
			uint8_t j;
			char div = cmd[i];
			
			j = i + 1;
			argv_tmp = &cmd[j];
			while (cmd[j] != div)
			{
				if ('\0' == cmd[j])
				{
					return VSFERR_FAIL;
				}
				j++;
			}
			i = j;
		}
		else
		{
			argv_tmp = &cmd[i];
			while (!isspace((int)cmd[i]) && (cmd[i] != '\0'))
			{
				i++;
			}
		}
		
		cmd[i++] = '\0';
		if (argv_num >= param->argc)
		{
			// can not accept more argv
			break;
		}
		
		// allocate buffer for argv_tmp
		param->argv[argv_num] = STRDUP(argv_tmp);
		if (NULL == param->argv[argv_num])
		{
			return VSFERR_FAIL;
		}
		argv_num++;
	}
	param->argc = argv_num;
	return VSFERR_NONE;
}

static struct vsfshell_handler_t *
vsfshell_search_handler(struct vsfshell_t *shell, char *name)
{
	struct vsfshell_handler_t *handler = shell->handlers;
	
	while (handler != NULL)
	{
		if (!strcmp(handler->name, name))
		{
			break;
		}
		handler = handler->next;
	}
	
	return handler;
}

static vsf_err_t
vsfshell_new_handler_thread(struct vsfshell_t *shell, char *cmd)
{
	struct vsfshell_handler_param_t *param =
		(struct vsfshell_handler_param_t *)MALLOC(sizeof(*param));
	struct vsfshell_handler_t *handler;
	uint32_t i;
	vsf_err_t err = VSFERR_NONE;
	
	if (NULL == param)
	{
		goto exit;
	}
	memset(param, 0, sizeof(*param));
	param->shell = shell;
	param->output_pt.sm = &param->sm;
	param->output_pt.thread = (vsfsm_pt_thread_t)vsfshell_output_thread;
	
	// parse command line
	param->argc = dimof(param->argv);
	if (vsfshell_parse_cmd(cmd, param))
	{
		goto exit_free_argv;
	}
	
	// search handler
	handler =  vsfshell_search_handler(shell, param->argv[0]);
	if (NULL == handler)
	{
		goto exit_free_argv;
	}
	
	param->pt.thread = handler->thread;
	param->pt.user_data = param;
	param->pt.sm = &param->sm;
	
	if (vsfsm_add_subsm(&shell->sm.init_state, &param->sm))
	{
		goto exit_free_argv;
	}
	shell->frontend_sm = &param->sm;
	vsfsm_pt_init(&param->sm, &param->pt, false);
	goto exit;
	
exit_free_argv:
	for (i = 0; i < dimof(param->argv); i++)
	{
		if (param->argv[i] != NULL)
		{
			FREE(param->argv[i]);
		}
	}
	FREE(param);
	err = VSFERR_FAIL;
exit:
	return err;
}

static void
vsfshell_free_handler_thread(struct vsfshell_t *shell, struct vsfsm_t *sm)
{
	if (sm != NULL)
	{
		struct vsfsm_pt_t *pt = (struct vsfsm_pt_t *)sm->user_data;
		
		if (sm->evtq.evt_buffer != NULL)
		{
			FREE(sm->evtq.evt_buffer);
		}
		if (pt != NULL)
		{
			if (pt->user_data != NULL)
			{
				FREE(pt->user_data);
			}
			FREE(pt);
		}
		FREE(sm);
	}
}

void vsfshell_handler_release_io(struct vsfsm_pt_t *pt)
{
	struct vsfshell_handler_param_t *param =
						(struct vsfshell_handler_param_t *)pt->user_data;
	struct vsfshell_t *shell = param->shell;
	
	if (shell->frontend_sm == &param->sm)
	{
		// current frontend_handler exit
		vsfsm_post_evt(&shell->sm, VSFSHELL_EVT_FRONT_HANDLER_EXIT);
	}
}

void vsfshell_handler_exit(struct vsfsm_pt_t *pt)
{
	struct vsfshell_handler_param_t *param =
						(struct vsfshell_handler_param_t *)pt->user_data;
	struct vsfshell_t *shell = param->shell;
	
	vsfshell_handler_release_io(pt);
	vsfsm_remove_subsm(&shell->sm.init_state, &param->sm);
	vsfshell_free_handler_thread(shell, &param->sm);
}

// vsfshell_input_thread is used to process the events
// 		from the sender of the stream_rx
vsf_err_t vsfshell_input_thread(struct vsfsm_pt_t *pt, vsfsm_evt_t evt)
{
	struct vsfshell_t *shell = (struct vsfshell_t *)pt->user_data;
	struct vsfsm_pt_t *output_pt = &shell->output_pt;
	char *cmd = (char *)shell->tbuffer.buffer.buffer;
	struct vsf_buffer_t buffer;
	uint8_t ch;
	
	switch (evt)
	{
	case VSFSHELL_EVT_STREAMTX_ONCONN:
		vsfshell_printf(shell, output_pt,
						"vsfshell 0.1 beta by SimonQian" VSFSHELL_LINEEND);
		// fall through
	case VSFSHELL_EVT_FRONT_HANDLER_EXIT:
		vsfshell_printf(shell, output_pt, VSFSHELL_LINEEND VSFSHELL_PROMPT);
		shell->frontend_sm = pt->sm;
		break;
	case VSFSHELL_EVT_STREAMRX_ONIN:
		do
		{
			buffer.buffer = &ch;
			buffer.size = 1;
			buffer.size = stream_rx(shell->stream_rx, &buffer);
			if (0 == buffer.size)
			{
				break;
			}
			
			if ('\r' == ch)
			{
				vsfshell_printf(shell, output_pt, "\n");
			}
			else if ('\b' == ch)
			{
				if (shell->tbuffer.position)
				{
					shell->tbuffer.position--;
					vsfshell_printf(shell, output_pt, "\b \b");
				}
				continue;
			}
			else if (!((ch >= ' ') && (ch <= '~')) ||
				(shell->tbuffer.position >= shell->tbuffer.buffer.size - 1))
			{
				continue;
			}
			else
			{
				shell->tbuffer.buffer.buffer[shell->tbuffer.position++] = ch;
			}
			
			// echo
			vsfshell_printf(shell, output_pt, "%c", ch);
			
			if ('\r' == ch)
			{
				if (shell->tbuffer.position > 0)
				{
					// create new handler thread
					cmd[shell->tbuffer.position] = '\0';
					if (vsfshell_new_handler_thread(shell, cmd))
					{
						vsfshell_printf(shell, output_pt,
							"Fail to execute : %s" VSFSHELL_LINEEND, cmd);
						vsfsm_post_evt(&shell->sm, VSFSHELL_EVT_FRONT_HANDLER_EXIT);
					}
				}
				shell->tbuffer.position = 0;
				break;
			}
		} while (buffer.size > 0);
		break;
	}
	return VSFERR_NOT_READY;
}

static struct vsfsm_state_t *
vsfshell_evt_handler(struct vsfsm_t *sm, vsfsm_evt_t evt)
{
	struct vsfshell_t *shell = (struct vsfshell_t *)sm->user_data;
	
	switch (evt)
	{
	case VSFSM_EVT_INIT:
		shell->output_interrupted = false;
		shell->tbuffer.buffer.buffer = (uint8_t *)shell->cmd_buff;
		shell->tbuffer.buffer.size = sizeof(shell->cmd_buff);
		shell->tbuffer.position = 0;
		vsfsm_crit_init(&shell->output_crit, VSFSHELL_EVT_OUTPUT_CRIT_AVAIL);
		
		shell->stream_rx->callback_rx.param = shell;
		shell->stream_rx->callback_rx.on_in_int =
							vsfshell_streamrx_callback_on_in_int;
		shell->stream_rx->callback_rx.on_connect_tx =
							vsfshell_streamrx_callback_on_txconn;
		shell->stream_tx->callback_tx.param = shell;
		shell->stream_tx->callback_tx.on_out_int =
							vsfshell_streamtx_callback_on_out_int;
		shell->stream_tx->callback_tx.on_connect_rx =
							vsfshell_streamtx_callback_on_rxconn;
		
		vsfshell_register_handlers(shell, vsfshell_handlers);
		shell->input_pt.user_data = shell;
		shell->input_pt.thread = vsfshell_input_thread;
		shell->input_pt.sm = sm;
		shell->output_pt.thread = (vsfsm_pt_thread_t)vsfshell_output_thread;
		shell->output_pt.sm = sm;
		
		stream_connect_rx(shell->stream_rx);
		stream_connect_tx(shell->stream_tx);
		if (shell->stream_rx->tx_ready)
		{
			vsfsm_post_evt(sm, VSFSHELL_EVT_STREAMRX_ONCONN);
		}
		if (shell->stream_tx->rx_ready)
		{
			vsfsm_post_evt(sm, VSFSHELL_EVT_STREAMTX_ONCONN);
		}
		break;
	case VSFSHELL_EVT_STREAMRX_ONCONN:
		break;
	case VSFSHELL_EVT_FRONT_HANDLER_EXIT:
	case VSFSHELL_EVT_STREAMTX_ONCONN:
		// pass to shell->input_pt
		shell->input_pt.thread(&shell->input_pt, evt);
		break;
	case VSFSHELL_EVT_STREAMRX_ONIN:
		// pass to shell->frontend_handler
		if (shell->frontend_sm == &shell->sm)
		{
			shell->input_pt.thread(&shell->input_pt, evt);
		}
		else if (shell->frontend_sm != NULL)
		{
			vsfsm_post_evt(shell->frontend_sm, evt);
		}
		break;
	case VSFSHELL_EVT_STREAMTX_ONOUT:
		// pass to shell->output_sm
		if (shell->output_sm == &shell->sm)
		{
			shell->output_pt.thread(&shell->output_pt, evt);
		}
		else if (shell->output_sm != NULL)
		{
			vsfsm_post_evt(shell->output_sm, evt);
		}
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

void vsfshell_register_handlers(struct vsfshell_t *shell,
										struct vsfshell_handler_t *handlers)
{
	while ((handlers != NULL) && (handlers->name != NULL))
	{
		handlers->next = shell->handlers;
		shell->handlers = handlers;
		
		handlers++;
	}
}

// handlers
static vsf_err_t
vsfshell_echo_handler(struct vsfsm_pt_t *pt, vsfsm_evt_t evt)
{
	struct vsfshell_handler_param_t *param =
						(struct vsfshell_handler_param_t *)pt->user_data;
	struct vsfsm_pt_t *output_pt = &param->output_pt;
	
	vsfsm_pt_begin(pt);
	if (param->argc != 2)
	{
		vsfshell_printf(param->shell, output_pt,
						"invalid format." VSFSHELL_LINEEND);
		vsfshell_printf(param->shell, output_pt,
						"format: echo STRING" VSFSHELL_LINEEND);
		goto handler_thread_end;
	}
	
	vsfshell_printf(param->shell, output_pt, "%s", param->argv[1]);
	vsfsm_pt_end(pt);
	
handler_thread_end:
	vsfshell_handler_exit(pt);
	return VSFERR_NONE;
}

static vsf_err_t
vsfshell_delayms_handler(struct vsfsm_pt_t *pt, vsfsm_evt_t evt)
{
	struct vsfshell_handler_param_t *param =
						(struct vsfshell_handler_param_t *)pt->user_data;
	struct vsfsm_pt_t *output_pt = &param->output_pt;
	
	vsfsm_pt_begin(pt);
	if (param->argc != 2)
	{
		vsfshell_printf(param->shell, output_pt,
						"invalid format." VSFSHELL_LINEEND);
		vsfshell_printf(param->shell, output_pt,
						"format: delayms MS" VSFSHELL_LINEEND);
		goto handler_thread_end;
	}
	
	param->priv = MALLOC(sizeof(struct vsftimer_timer_t));
	if (NULL == param->priv)
	{
		vsfshell_printf(param->shell, output_pt,
						"not enough resources." VSFSHELL_LINEEND);
		goto handler_thread_end;
	}
	memset(param->priv, 0, sizeof(struct vsftimer_timer_t));
	((struct vsftimer_timer_t *)param->priv)->sm = pt->sm;
	((struct vsftimer_timer_t *)param->priv)->evt = VSFSM_EVT_USER_LOCAL_INSTANT;
	((struct vsftimer_timer_t *)param->priv)->interval = strtoul(param->argv[1], NULL, 0);
	vsftimer_register((struct vsftimer_timer_t *)param->priv);
	vsfsm_pt_wfe(pt, VSFSM_EVT_USER_LOCAL_INSTANT);
	vsftimer_unregister((struct vsftimer_timer_t *)param->priv);
	
	vsfsm_pt_end(pt);
	
handler_thread_end:
	if (param->priv != NULL)
	{
		FREE(param->priv);
	}
	vsfshell_handler_exit(pt);
	return VSFERR_NONE;
}
