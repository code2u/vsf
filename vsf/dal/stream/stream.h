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

#ifndef __STREAM_H_INCLUDED__
#define __STREAM_H_INCLUDED__

#include "tool/buffer/buffer.h"

struct vsf_stream_t
{
	struct vsf_fifo_t fifo;
	// callback_tx is notification for tx end of the stream
	// when rx end read the data out, will notify the tx end
	struct
	{
		void *param;
		void (*on_out_int)(void *param);
		void (*on_connect_rx)(void *param);
	} callback_tx;
	// callback_rx is notification for rx end of the stream
	// when tx end write the data in, will notify the rx end
	struct
	{
		void *param;
		void (*on_in_int)(void *param);
		void (*on_connect_tx)(void *param);
	} callback_rx;
	bool tx_ready;
	bool rx_ready;
	bool overflow;
};

vsf_err_t stream_init(struct vsf_stream_t *stream);
vsf_err_t stream_fini(struct vsf_stream_t *stream);
uint32_t stream_rx(struct vsf_stream_t *stream, struct vsf_buffer_t *buffer);
uint32_t stream_tx(struct vsf_stream_t *stream, struct vsf_buffer_t *buffer);
uint32_t stream_get_data_size(struct vsf_stream_t *stream);
uint32_t stream_get_free_size(struct vsf_stream_t *stream);
void stream_connect_rx(struct vsf_stream_t *stream);
void stream_connect_tx(struct vsf_stream_t *stream);

#endif	// __STREAM_H_INCLUDED__
