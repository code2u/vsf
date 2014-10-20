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

static void vsfshell_print_string(struct vsfshell_t *shell, char *str)
{
	struct vsf_buffer_t buffer;
	
	buffer.buffer = (uint8_t *)str;
	buffer.size = strlen(str);
	stream_tx(shell->stream_tx, &buffer);
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
		(struct vsfshell_handler_param_t *)malloc(sizeof(*param));
	struct vsfshell_handler_t *handler;
	uint32_t i;
	bool frontend = false;
	vsf_err_t err = VSFERR_NONE;
	
	if (NULL == param)
	{
		goto exit;
	}
	memset(param, 0, sizeof(*param));
	param->shell = shell;
	
	// parse command line
	param->argc = dimof(param->argv);
	if (vsfshell_parse_cmd(cmd, param))
	{
		goto exit_free_argv;
	}
	
	if ((param->argc > 1) && !strcmp(param->argv[param->argc - 1], "&"))
	{
		// background thread
		param->argc--;
		FREE(param->argv[param->argc - 1]);
		param->argv[param->argc - 1] = NULL;
	}
	else
	{
		// frontend thread
		frontend = true;
	}
	
	// search handler
	handler =  vsfshell_search_handler(shell, param->argv[0]);
	if (NULL == handler)
	{
		goto exit_free_argv;
	}
	
	param->pt.thread = handler->thread;
	param->pt.user_data = param;
	
	if (vsfsm_add_subsm(&shell->sm.init_state, &param->sm))
	{
		goto exit_free_argv;
	}
	if (frontend)
	{
		shell->frontend_handler = &param->sm;
	}
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
	param = NULL;
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

void vsfshell_handler_exit(struct vsfsm_pt_t *pt)
{
	struct vsfshell_handler_param_t *param =
						(struct vsfshell_handler_param_t *)pt->user_data;
	struct vsfshell_t *shell = param->shell;
	
	if (shell->frontend_handler == &param->sm)
	{
		// current frontend_handler exit
		shell->frontend_handler = NULL;
		vsfshell_print_string(shell, VSFSHELL_LINEEND VSFSHELL_PROMPT);
	}
	vsfsm_remove_subsm(&shell->sm.init_state, &param->sm);
	vsfshell_free_handler_thread(shell, &param->sm);
}

static struct vsfsm_state_t *
vsfshell_evt_handler(struct vsfsm_t *sm, vsfsm_evt_t evt)
{
	struct vsfshell_t *shell = (struct vsfshell_t *)sm->user_data;
	
	switch (evt)
	{
	case VSFSM_EVT_INIT:
		shell->tbuffer.buffer.buffer = (uint8_t *)shell->cmd_buff;
		shell->tbuffer.buffer.size = sizeof(shell->cmd_buff);
		shell->tbuffer.position = 0;
		
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
	case VSFSHELL_EVT_STREAMTX_ONCONN:
		vsfshell_print_string(shell, "vsfshell 0.1 beta" VSFSHELL_LINEEND);
		vsfshell_print_string(shell, VSFSHELL_PROMPT);
		break;
	case VSFSHELL_EVT_STREAMRX_ONIN:
		if (shell->frontend_handler != NULL)
		{
			// send input to front_handler
		}
		else
		{
			struct vsf_buffer_t buffer;
			uint8_t ch;
			
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
					vsfshell_print_string(shell, "\n");
				}
				else if ('\b' == ch)
				{
					if (shell->tbuffer.position)
					{
						shell->tbuffer.position--;
						vsfshell_print_string(shell, "\b \b");
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
				stream_tx(shell->stream_tx, &buffer);
				
				if ('\r' == ch)
				{
					if (shell->tbuffer.position > 0)
					{
						char *cmd = (char *)shell->tbuffer.buffer.buffer;
						// create new handler thread
						cmd[shell->tbuffer.position] = '\0';
						if (vsfshell_new_handler_thread(shell, cmd))
						{
							vsfshell_print_string(shell, "Fail to execute :");
							vsfshell_print_string(shell, cmd);
							vsfshell_print_string(shell, VSFSHELL_LINEEND);
						}
					}
					if (NULL == shell->frontend_handler)
					{
						vsfshell_print_string(shell, VSFSHELL_PROMPT);
					}
					shell->tbuffer.position = 0;
					break;
				}
			} while (buffer.size > 0);
		}
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
	struct vsfshell_t *shell = param->shell;
	struct vsf_buffer_t buffer;
	
	vsfsm_pt_begin(pt);
	if (param->argc != 2)
	{
		vsfshell_print_string(shell, "invalid format." VSFSHELL_LINEEND);
		vsfshell_print_string(shell, "format: echo STRING" VSFSHELL_LINEEND);
		goto handler_thread_end;
	}
	
	buffer.buffer = (uint8_t *)param->argv[1];
	buffer.size = strlen(param->argv[1]);
	stream_tx(shell->stream_tx, &buffer);
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
	struct vsfshell_t *shell = param->shell;
	
	vsfsm_pt_begin(pt);
	if (param->argc != 2)
	{
		vsfshell_print_string(shell, "invalid format." VSFSHELL_LINEEND);
		vsfshell_print_string(shell, "format: delayms MS" VSFSHELL_LINEEND);
		goto handler_thread_end;
	}
	
	param->priv = MALLOC(sizeof(struct vsftimer_timer_t));
	if (NULL == param->priv)
	{
		vsfshell_print_string(shell, "not enough resources." VSFSHELL_LINEEND);
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
