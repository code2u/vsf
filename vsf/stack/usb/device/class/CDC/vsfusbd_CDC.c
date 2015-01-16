#include "app_cfg.h"
#include "interfaces.h"

#include "stack/usb/usb_common.h"
#include "stack/usb/device/vsfusbd.h"

#include "vsfusbd_CDC.h"

enum vsfusbd_CDC_EVT_t
{
	VSFUSBD_CDC_EVT_STREAMTX_ONIN = VSFSM_EVT_USER_LOCAL + 0,
	VSFUSBD_CDC_EVT_STREAMRX_ONOUT = VSFSM_EVT_USER_LOCAL + 1,
	VSFUSBD_CDC_EVT_STREAMTX_ONCONN = VSFSM_EVT_USER_LOCAL + 2,
	VSFUSBD_CDC_EVT_STREAMRX_ONCONN = VSFSM_EVT_USER_LOCAL + 3,
};

static vsf_err_t vsfusbd_CDCData_OUT_hanlder(struct vsfusbd_device_t *device,
												uint8_t ep)
{
	struct vsfusbd_config_t *config = &device->config[device->configuration];
	int8_t iface = config->ep_OUT_iface_map[ep];
	struct vsfusbd_CDC_param_t *param = NULL;
	uint16_t pkg_size, ep_size;
	uint8_t buffer[64];
	struct vsf_buffer_t rx_buffer;
	
	if (iface < 0)
	{
		return VSFERR_FAIL;
	}
	param = (struct vsfusbd_CDC_param_t *)config->iface[iface].protocol_param;
	if (NULL == param)
	{
		return VSFERR_FAIL;
	}
	
	ep_size = device->drv->ep.get_OUT_epsize(ep);
	pkg_size = device->drv->ep.get_OUT_count(ep);
	if (pkg_size > ep_size)
	{
		return VSFERR_FAIL;
	}
	device->drv->ep.read_OUT_buffer(ep, buffer, pkg_size);
	
	rx_buffer.buffer = buffer;
	rx_buffer.size = pkg_size;
	stream_tx(param->stream_rx, &rx_buffer);
	
	if (stream_get_free_size(param->stream_rx) < ep_size)
	{
		param->out_enable = false;
	}
	else
	{
		device->drv->ep.enable_OUT(ep);
	}
	
	return VSFERR_NONE;
}

static vsf_err_t vsfusbd_CDCData_IN_hanlder(struct vsfusbd_device_t *device,
											uint8_t ep)
{
	struct vsfusbd_config_t *config = &device->config[device->configuration];
	int8_t iface = config->ep_IN_iface_map[ep];
	struct vsfusbd_CDC_param_t *param = NULL;
	uint16_t pkg_size;
	uint8_t buffer[64];
	uint32_t tx_data_length;
	struct vsf_buffer_t tx_buffer;
	
	if (iface < 0)
	{
		return VSFERR_FAIL;
	}
	param = (struct vsfusbd_CDC_param_t *)config->iface[iface].protocol_param;
	if (NULL == param)
	{
		return VSFERR_FAIL;
	}
	
	pkg_size = device->drv->ep.get_IN_epsize(ep);
	tx_buffer.buffer = buffer;
	tx_buffer.size = pkg_size;
	tx_data_length = stream_rx(param->stream_tx, &tx_buffer);
	if (tx_data_length)
	{
		device->drv->ep.write_IN_buffer(ep, buffer, tx_data_length);
		device->drv->ep.set_IN_count(ep, tx_data_length);
	}
	else
	{
		param->in_enable = false;
	}
	
	return VSFERR_NONE;
}

static void vsfusbd_CDCData_streamtx_callback_on_in_int(void *p)
{
	struct vsfusbd_CDC_param_t *param = (struct vsfusbd_CDC_param_t *)p;
	
	vsfsm_post_evt_pending(&param->iface->sm, VSFUSBD_CDC_EVT_STREAMTX_ONIN);
}

static void vsfusbd_CDCData_streamrx_callback_on_out_int(void *p)
{
	struct vsfusbd_CDC_param_t *param = (struct vsfusbd_CDC_param_t *)p;
	
	vsfsm_post_evt_pending(&param->iface->sm, VSFUSBD_CDC_EVT_STREAMRX_ONOUT);
}

static void vsfusbd_CDCData_streamtx_callback_on_txconn(void *p)
{
	struct vsfusbd_CDC_param_t *param = (struct vsfusbd_CDC_param_t *)p;
	
	vsfsm_post_evt_pending(&param->iface->sm, VSFUSBD_CDC_EVT_STREAMTX_ONCONN);
}

static void vsfusbd_CDCData_streamrx_callback_on_rxconn(void *p)
{
	struct vsfusbd_CDC_param_t *param = (struct vsfusbd_CDC_param_t *)p;
	
	vsfsm_post_evt_pending(&param->iface->sm, VSFUSBD_CDC_EVT_STREAMRX_ONCONN);
}

static struct vsfsm_state_t *
vsfusbd_CDCData_evt_handler(struct vsfsm_t *sm, vsfsm_evt_t evt)
{
	struct vsfusbd_CDC_param_t *param =
						(struct vsfusbd_CDC_param_t *)sm->user_data;
	struct vsfusbd_device_t *device = param->device;
	
	switch (evt)
	{
	case VSFSM_EVT_INIT:
		param->stream_tx->callback_rx.param = param;
		param->stream_tx->callback_rx.on_in_int =
							vsfusbd_CDCData_streamtx_callback_on_in_int;
		param->stream_tx->callback_rx.on_connect_tx =
							vsfusbd_CDCData_streamtx_callback_on_txconn;
		param->stream_rx->callback_tx.param = param;
		param->stream_rx->callback_tx.on_out_int =
							vsfusbd_CDCData_streamrx_callback_on_out_int;
		param->stream_rx->callback_tx.on_connect_rx =
							vsfusbd_CDCData_streamrx_callback_on_rxconn;
		
		param->out_enable = false;
		param->in_enable = false;
		break;
	case VSFUSBD_CDC_EVT_STREAMTX_ONCONN:
		vsfusbd_set_IN_handler(device, param->ep_in,
										vsfusbd_CDCData_IN_hanlder);
		break;
	case VSFUSBD_CDC_EVT_STREAMRX_ONCONN:
		vsfusbd_set_OUT_handler(device, param->ep_out,
										vsfusbd_CDCData_OUT_hanlder);
		vsfsm_post_evt(sm, VSFUSBD_CDC_EVT_STREAMRX_ONOUT);
		break;
	case VSFUSBD_CDC_EVT_STREAMTX_ONIN:
		if (!param->in_enable)
		{
			param->in_enable = true;
			vsfusbd_CDCData_IN_hanlder(param->device, param->ep_in);
		}
		break;
	case VSFUSBD_CDC_EVT_STREAMRX_ONOUT:
		if (!param->out_enable &&
			(stream_get_free_size(param->stream_rx) >=
					device->drv->ep.get_OUT_epsize(param->ep_out)))
		{
			param->out_enable = true;
			device->drv->ep.enable_OUT(param->ep_out);
		}
		break;
	}
	
	return NULL;
}

static vsf_err_t vsfusbd_CDCData_class_init(uint8_t iface, 
											struct vsfusbd_device_t *device)
{
	struct vsfusbd_config_t *config = &device->config[device->configuration];
	struct vsfusbd_iface_t *ifs = &config->iface[iface];
	struct vsfusbd_CDC_param_t *param = 
						(struct vsfusbd_CDC_param_t *)ifs->protocol_param;
	
	if ((NULL == param) || (NULL == param->stream_tx) ||
		(NULL == param->stream_rx))
	{
		return VSFERR_INVALID_PARAMETER;
	}
	
	// state machine init
	ifs->sm.init_state.evt_handler = vsfusbd_CDCData_evt_handler;
	param->iface = ifs;
	param->device = device;
	ifs->sm.user_data = (void*)param;
	return vsfsm_init(&ifs->sm);
}

static vsf_err_t vsfusbd_CDCControl_SendEncapsulatedCommand_prepare(
	struct vsfusbd_device_t *device, struct vsf_buffer_t *buffer,
		uint8_t* (*data_io)(void *param))
{
	struct usb_ctrl_request_t *request = &device->ctrl_handler.request;
	uint8_t iface = request->index;
	struct vsfusbd_config_t *config = &device->config[device->configuration];
	struct vsfusbd_CDC_param_t *param =
		(struct vsfusbd_CDC_param_t *)config->iface[iface].protocol_param;
	
	if (request->length > param->encapsulated_command_buffer.size)
	{
		return VSFERR_FAIL;
	}
	
	buffer->buffer = param->encapsulated_command_buffer.buffer;
	buffer->size = request->length;
	return VSFERR_NONE;
}

static vsf_err_t vsfusbd_CDCControl_SendEncapsulatedCommand_process(
	struct vsfusbd_device_t *device, struct vsf_buffer_t *buffer)
{
	struct usb_ctrl_request_t *request = &device->ctrl_handler.request;
	uint8_t iface = request->index;
	struct vsfusbd_config_t *config = &device->config[device->configuration];
	struct vsfusbd_CDC_param_t *param =
		(struct vsfusbd_CDC_param_t *)config->iface[iface].protocol_param;
	
	if ((param->callback.send_encapsulated_command != NULL) &&
		param->callback.send_encapsulated_command(buffer))
	{
		return VSFERR_FAIL;
	}
	return VSFERR_NONE;
}

static vsf_err_t vsfusbd_CDCControl_GetEncapsulatedResponse_prepare(
	struct vsfusbd_device_t *device, struct vsf_buffer_t *buffer,
		uint8_t* (*data_io)(void *param))
{
	struct usb_ctrl_request_t *request = &device->ctrl_handler.request;
	uint8_t iface = request->index;
	struct vsfusbd_config_t *config = &device->config[device->configuration];
	struct vsfusbd_CDC_param_t *param =
		(struct vsfusbd_CDC_param_t *)config->iface[iface].protocol_param;
	
	if (request->length > param->encapsulated_response_buffer.size)
	{
		return VSFERR_FAIL;
	}
	
	buffer->buffer = param->encapsulated_response_buffer.buffer;
	buffer->size = request->length;
	return VSFERR_NONE;
}

static const struct vsfusbd_setup_filter_t vsfusbd_CDCControl_class_setup[] = 
{
/*	{
		USB_REQ_DIR_HTOD | USB_REQ_TYPE_CLASS | USB_REQ_RECP_INTERFACE,
		USB_CDCREQ_SET_COMM_FEATURE,
		vsfusbd_CDCControl_SetCommFeature_prepare,
		NULL
	},
	{
		USB_REQ_DIR_DTOH | USB_REQ_TYPE_CLASS | USB_REQ_RECP_INTERFACE,
		USB_CDCREQ_GET_COMM_FEATURE,
		vsfusbd_CDCControl_GetCommFeature_prepare,
		NULL
	},
	{
		USB_REQ_DIR_HTOD | USB_REQ_TYPE_CLASS | USB_REQ_RECP_INTERFACE,
		USB_CDCREQ_CLEAR_COMM_FEATURE,
		vsfusbd_CDCControl_ClearCommFeature_prepare,
		NULL
	},
*/	{
		USB_REQ_DIR_HTOD | USB_REQ_TYPE_CLASS | USB_REQ_RECP_INTERFACE,
		USB_CDCREQ_SEND_ENCAPSULATED_COMMAND,
		vsfusbd_CDCControl_SendEncapsulatedCommand_prepare,
		vsfusbd_CDCControl_SendEncapsulatedCommand_process,
	},
	{
		USB_REQ_DIR_DTOH | USB_REQ_TYPE_CLASS | USB_REQ_RECP_INTERFACE,
		USB_CDCREQ_GET_ENCAPSULATED_RESPONSE,
		vsfusbd_CDCControl_GetEncapsulatedResponse_prepare,
		NULL,
	},
	VSFUSBD_SETUP_NULL
};

void vsfusbd_CDCData_connect(struct vsfusbd_CDC_param_t *param)
{
	stream_connect_tx(param->stream_rx);
	stream_connect_rx(param->stream_tx);
}

const struct vsfusbd_class_protocol_t vsfusbd_CDCControl_class = 
{
	NULL, NULL,
	(struct vsfusbd_setup_filter_t *)vsfusbd_CDCControl_class_setup, NULL,
	
	NULL, NULL
};

const struct vsfusbd_class_protocol_t vsfusbd_CDCData_class = 
{
	NULL, NULL, NULL, NULL,
	
	vsfusbd_CDCData_class_init, NULL
};
