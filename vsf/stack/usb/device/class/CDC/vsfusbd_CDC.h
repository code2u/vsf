#ifndef __VSFUSBD_CDC_H_INCLUDED__
#define __VSFUSBD_CDC_H_INCLUDED__

#include "tool/buffer/buffer.h"
#include "dal/stream/stream.h"

enum usb_CDC_req_t
{
	USB_CDCREQ_SEND_ENCAPSULATED_COMMAND	= 0x00,
	USB_CDCREQ_GET_ENCAPSULATED_RESPONSE	= 0x01,
	USB_CDCREQ_SET_COMM_FEATURE				= 0x02,
	USB_CDCREQ_GET_COMM_FEATURE				= 0x03,
	USB_CDCREQ_CLEAR_COMM_FEATURE			= 0x04,
};

extern const struct vsfusbd_class_protocol_t vsfusbd_CDCControl_class;
extern const struct vsfusbd_class_protocol_t vsfusbd_CDCData_class;

struct vsfusbd_CDC_param_t
{
	uint8_t ep_out;
	uint8_t ep_in;
	
	// stream_tx is used for data stream send to USB host
	struct vsf_stream_t *stream_tx;
	// stream_rx is used for data stream receive from USB host
	struct vsf_stream_t *stream_rx;
	
	struct
	{
		vsf_err_t (*send_encapsulated_command)(struct vsf_buffer_t *buffer);
	} callback;
	
	// no need to initialize below if encapsulate command/response is not used
	struct vsf_buffer_t encapsulated_command_buffer;
	struct vsf_buffer_t encapsulated_response_buffer;
	
	// no need to initialize below by user
	bool out_enable;
	bool in_enable;
	
	struct vsfusbd_device_t *device;
	struct vsfusbd_iface_t *iface;
};

// helper functions
struct vsfusbd_setup_filter_t *vsfusbd_get_request_filter_do(
		struct vsfusbd_device_t *device, struct vsfusbd_setup_filter_t *list);

#endif	// __VSFUSBD_CDC_H_INCLUDED__
