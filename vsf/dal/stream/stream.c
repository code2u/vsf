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

#include "app_cfg.h"
#include "app_type.h"

#include "stream.h"

vsf_err_t stream_init(struct vsf_stream_t *stream)
{
	stream->overflow = false;
	vsf_fifo_init(&stream->fifo);
	return VSFERR_NONE;
}

vsf_err_t stream_fini(struct vsf_stream_t *stream)
{
	REFERENCE_PARAMETER(stream);
	return VSFERR_NONE;
}

uint32_t stream_rx(struct vsf_stream_t *stream, struct vsf_buffer_t *buffer)
{
	uint32_t count = vsf_fifo_pop(&stream->fifo, buffer->size, buffer->buffer);
	
	if ((stream->callback_tx.on_out_int != NULL) && (count > 0))
	{
		stream->callback_tx.on_out_int(stream->callback_tx.param);
	}
	return count;
}

uint32_t stream_tx(struct vsf_stream_t *stream, struct vsf_buffer_t *buffer)
{
	uint32_t count = vsf_fifo_push(&stream->fifo, buffer->size, buffer->buffer);
	
	if (count < buffer->size)
	{
		stream->overflow = true;
	}
	if ((stream->callback_rx.on_in_int != NULL) && (count > 0))
	{
		stream->callback_rx.on_in_int(stream->callback_rx.param);
	}
	return count;
}

uint32_t stream_get_data_size(struct vsf_stream_t *stream)
{
	return vsf_fifo_get_data_length(&stream->fifo);
}

uint32_t stream_get_free_size(struct vsf_stream_t *stream)
{
	return vsf_fifo_get_avail_length(&stream->fifo);
}
