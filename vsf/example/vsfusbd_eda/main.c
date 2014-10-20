#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "compiler.h"
#include "app_cfg.h"
#include "app_type.h"

#include "interfaces.h"
#include "framework/vsfsm/vsfsm.h"
#include "framework/vsftimer/vsftimer.h"

#include "dal/stream/stream.h"

#include "stack/usb/device/vsfusbd.h"
#include "stack/usb/device/class/HID/vsfusbd_HID.h"
#include "stack/usb/device/class/CDC/vsfusbd_CDCACM.h"

#include "vsfshell/vsfshell.h"

// USB descriptors
static const uint8_t USB_DeviceDescriptor[] =
{
	0x12,	// bLength = 18
	USB_DESC_TYPE_DEVICE,	// USB_DESC_TYPE_DEVICE
	0x00,
	0x02,	// bcdUSB
	0xEF,	// device class: IAD
	0x02,	// device sub class
	0x01,	// device protocol
	0x40,	// max packet size
	0x83,
	0x04,	// vendor
	0x39,
	0xA0,	// product
	0x00,
	0x02,	// bcdDevice
	1,	// manu facturer
	2,	// product
	3,	// serial number
	0x01	// number of configuration 
};

static const uint8_t USB_ConfigDescriptor[] =
{
	// Configuation Descriptor
	0x09,	// bLength: Configuation Descriptor size
	USB_DESC_TYPE_CONFIGURATION,
			// bDescriptorType: Configuration
	108,	// wTotalLength:no of returned bytes*
	0x00,
	0x03,	// bNumInterfaces: 1 interface
	0x01,	// bConfigurationValue: Configuration value
	0x00,	// iConfiguration: Index of string descriptor describing the configuration
	0x80,	// bmAttributes: bus powered
	0x64,	// MaxPower 200 mA
	
	// IAD
	0x08,	// bLength: IAD Descriptor size
	USB_DESC_TYPE_IAD,
			// bDescriptorType: IAD
	0,		// bFirstInterface
	1,		// bInterfaceCount
	0x03,	// bFunctionClass
	0x01,	// bFunctionSubClass
	0x01,	// bFunctionProtocol
	0x04,	// iFunction

	// Interface Descriptor for HID
	0x09,	// bLength: Interface Descriptor size
	USB_DESC_TYPE_INTERFACE,	// bDescriptorType:
	0x00,	// bInterfaceNumber: Number of Interface
	0x00,	// bAlternateSetting: Alternate setting
	0x01,	// bNumEndpoints
	0x03,	// bInterfaceClass
	0x01,	// bInterfaceSubClass : 1=BOOT, 0=no boot*
	0x01,	// nInterfaceProtocol : 0=none, 1=keyboard, 2=mouse
	0x04,	// iInterface:
	
	// HID Descriptor
	0x09,
	USB_HIDDESC_TYPE_HID,	// 0x21 == HID Discriptor
	0x00,
	0x01,	// HID BCD ID
	0x00,	// Country Code
	0x01,	// Number of Descriptors
	USB_HIDDESC_TYPE_REPORT,	// Descriptor Type
	63,
	0x00,	// Descriptor Length
	
	// Endpoint 1 Descriptor
	0x07,	// Endpoint descriptor length = 7
	USB_DESC_TYPE_ENDPOINT,	// Endpoint descriptor type
	0x81,	// Endpoint address (IN, address 1)
	0x03,	// interrupt endpoint type
	0x40,	// Maximum packet size (64 bytes)
	0x00,
	0x20,	// Polling interval in milliseconds
	
	// IAD
	0x08,	// bLength: IAD Descriptor size
	USB_DESC_TYPE_IAD,
			// bDescriptorType: IAD
	1,		// bFirstInterface
	2,		// bInterfaceCount
	0x02,	// bFunctionClass
	0x02,	// bFunctionSubClass
	0x01,	// bFunctionProtocol
	0x05,	// iFunction
	
	// Interface Descriptor for CDC
	0x09,	// bLength: Interface Descriptor size
	USB_DESC_TYPE_INTERFACE,
			// bDescriptorType: Interface
	1,		// bInterfaceNumber: Number of Interface
	0x00,	// bAlternateSetting: Alternate setting
	0x01,	// bNumEndpoints: One endpoints used
	0x02,	// bInterfaceClass: Communication Interface Class
	0x02,	// bInterfaceSubClass: Abstract Control Model
	0x01,	// bInterfaceProtocol: Common AT commands
	0x05,	// iInterface:
	
	// Header Functional Descriptor
	0x05,	// bLength: Endpoint Descriptor size
	0x24,	// bDescriptorType: CS_INTERFACE
	0x00,	// bDescriptorSubtype: Header Func Desc
	0x10,	// bcdCDC: spec release number
	0x01,
	
	// Call Managment Functional Descriptor
	0x05,	// bFunctionLength
	0x24,	// bDescriptorType: CS_INTERFACE
	0x01,	// bDescriptorSubtype: Call Management Func Desc
	0x00,	// bmCapabilities: D0+D1
	0x01,	// bDataInterface: 1
	
	// ACM Functional Descriptor
	0x04,	// bFunctionLength
	0x24,	// bDescriptorType: CS_INTERFACE
	0x02,	// bDescriptorSubtype: Abstract Control Management desc
	0x02,	// bmCapabilities
	
	// Union Functional Descriptor
	0x05,	// bFunctionLength
	0x24,	// bDescriptorType: CS_INTERFACE
	0x06,	// bDescriptorSubtype: Union func desc
	1,		// bMasterInterface: Communication class interface
	2,		// bSlaveInterface0: Data Class Interface
	
	// Endpoint 2 Descriptor
	0x07,	// bLength: Endpoint Descriptor size
	USB_DESC_TYPE_ENDPOINT,
			// bDescriptorType: Endpoint
	0x82,	// bEndpointAddress: (IN2)
	0x03,	// bmAttributes: Interrupt
	8,		// wMaxPacketSize:
	0x00,
	0xFF,	// bInterval:
	
	// Data class interface descriptor
	0x09,	// bLength: Endpoint Descriptor size
	USB_DESC_TYPE_INTERFACE,
			// bDescriptorType: Interface
	2,		// bInterfaceNumber: Number of Interface
	0x00,	// bAlternateSetting: Alternate setting
	0x02,	// bNumEndpoints: Two endpoints used
	0x0A,	// bInterfaceClass: CDC
	0x00,	// bInterfaceSubClass:
	0x00,	// bInterfaceProtocol:
	0x00,	// iInterface:
	
	// Endpoint 3 Descriptor
	0x07,	// bLength: Endpoint Descriptor size
	USB_DESC_TYPE_ENDPOINT,
			// bDescriptorType: Endpoint
	0x03,	// bEndpointAddress: (OUT3)
	0x02,	// bmAttributes: Bulk
	64,		// wMaxPacketSize:
	0x00,
	0x00,	// bInterval: ignore for Bulk transfer
	
	// Endpoint 3 Descriptor
	0x07,	// bLength: Endpoint Descriptor size
	USB_DESC_TYPE_ENDPOINT,
			// bDescriptorType: Endpoint
	0x83,	// bEndpointAddress: (IN3)
	0x02,	// bmAttributes: Bulk
	64,		// wMaxPacketSize:
	0x00,
	0x00	// bInterval
};

static const uint8_t USB_StringLangID[] =
{
	4,
	USB_DESC_TYPE_STRING,
	0x09,
	0x04
};

static const uint8_t USB_StringVendor[] =
{
	20,
	USB_DESC_TYPE_STRING,
	'S', 0, 'i', 0, 'm', 0, 'o', 0, 'n', 0, 'Q', 0, 'i', 0, 'a', 0,
	'n', 0
};

static const uint8_t USB_StringSerial[50] =
{
	50,
	USB_DESC_TYPE_STRING,
	'0', 0, '1', 0, '2', 0, '3', 0, '4', 0, '5', 0, '6', 0, '7', 0, 
	'8', 0, '9', 0, 'A', 0, 'B', 0, 'C', 0, 'D', 0, 'E', 0, 'F', 0, 
	'0', 0, '0', 0, '0', 0, '0', 0, '0', 0, '0', 0, '0', 0, '0', 0, 
};

static const uint8_t USB_StringProduct[] =
{
	14,
	USB_DESC_TYPE_STRING,
	'V', 0, 'S', 0, 'F', 0, 'U', 0, 'S', 0, 'B', 0
};

static const uint8_t HID_StringFunc[] =
{
	14,
	USB_DESC_TYPE_STRING,
	'V', 0, 'S', 0, 'F', 0, 'H', 0, 'I', 0, 'D', 0
};

static const uint8_t CDC_StringFunc[] =
{
	14,
	USB_DESC_TYPE_STRING,
	'V', 0, 'S', 0, 'F', 0, 'C', 0, 'D', 0, 'C', 0
};

static const uint8_t HID_ReportDescriptor[63] = {
    0x05, 0x01, // USAGE_PAGE (Generic Desktop)
    0x09, 0x06, // USAGE (Keyboard)
    0xa1, 0x01, // COLLECTION (Application)
    0x05, 0x07, // USAGE_PAGE (Keyboard)
    0x19, 0xe0, // USAGE_MINIMUM (Keyboard LeftControl)
    0x29, 0xe7, // USAGE_MAXIMUM (Keyboard Right GUI)
    0x15, 0x00, // LOGICAL_MINIMUM (0)
    0x25, 0x01, // LOGICAL_MAXIMUM (1)
    0x75, 0x01, // REPORT_SIZE (1)
    0x95, 0x08, // REPORT_COUNT (8)
    0x81, 0x02, // INPUT (Data,Var,Abs)
    0x95, 0x01, // REPORT_COUNT (1)
    0x75, 0x08, // REPORT_SIZE (8)
    0x81, 0x03, // INPUT (Cnst,Var,Abs)
    0x95, 0x05, // REPORT_COUNT (5)
    0x75, 0x01, // REPORT_SIZE (1)
    0x05, 0x08, // USAGE_PAGE (LEDs)
    0x19, 0x01, // USAGE_MINIMUM (Num Lock)
    0x29, 0x05, // USAGE_MAXIMUM (Kana)
    0x91, 0x02, // OUTPUT (Data,Var,Abs)
    0x95, 0x01, // REPORT_COUNT (1)
    0x75, 0x03, // REPORT_SIZE (3)
    0x91, 0x03, // OUTPUT (Cnst,Var,Abs) 
    0x95, 0x06, // REPORT_COUNT (6)
    0x75, 0x08, // REPORT_SIZE (8)
    0x15, 0x00, // LOGICAL_MINIMUM (0)
    0x25, 0xFF, // LOGICAL_MAXIMUM (255)
    0x05, 0x07, // USAGE_PAGE (Keyboard)
    0x19, 0x00, // USAGE_MINIMUM (Reserved (no event indicated))
    0x29, 0x65, // USAGE_MAXIMUM (Keyboard Application)
    0x81, 0x00, // INPUT (Data,Ary,Abs)
    0xc0 // END_COLLECTION
};

static const struct vsfusbd_desc_filter_t HID_descriptors[] = 
{
	VSFUSBD_DESC_DEVICE(0, USB_DeviceDescriptor, sizeof(USB_DeviceDescriptor), NULL),
	VSFUSBD_DESC_CONFIG(0, 0, USB_ConfigDescriptor, sizeof(USB_ConfigDescriptor), NULL),
	VSFUSBD_DESC_STRING(0, 0, USB_StringLangID, sizeof(USB_StringLangID), NULL),
	VSFUSBD_DESC_STRING(0x0409, 1, USB_StringVendor, sizeof(USB_StringVendor), NULL),
	VSFUSBD_DESC_STRING(0x0409, 2, USB_StringProduct, sizeof(USB_StringProduct), NULL),
	VSFUSBD_DESC_STRING(0x0409, 3, USB_StringSerial, sizeof(USB_StringSerial), NULL),
	VSFUSBD_DESC_STRING(0x0409, 4, HID_StringFunc, sizeof(HID_StringFunc), NULL),
	VSFUSBD_DESC_STRING(0x0409, 5, CDC_StringFunc, sizeof(CDC_StringFunc), NULL),
	VSFUSBD_DESC_NULL
};

static const struct vsfusbd_desc_filter_t HID_Report_Descriptors[] =
{
	VSFUSBD_DESC_HID_REPORT(HID_ReportDescriptor, sizeof(HID_ReportDescriptor), NULL),
	VSFUSBD_DESC_NULL
};

static vsf_err_t HID_on_set_get_report(struct vsfusbd_HID_report_t *report)
{
	return VSFERR_NONE;
}

// app state machine events
#define APP_EVT_USBPU_TO				VSFSM_EVT_USER_LOCAL_INSTANT + 0

static struct vsfsm_state_t *
app_evt_handler(struct vsfsm_t *sm, vsfsm_evt_t evt);

struct vsfapp_t
{
	// hw
	struct usb_pullup_port_t
	{
		uint8_t port;
		uint8_t pin;
	} usb_pullup;
	
	struct
	{
		struct
		{
			struct vsfusbd_HID_param_t param;
			struct vsfusbd_HID_report_t reports[1];
			// private
			uint8_t report0_buffer[8];
		} hid;
		struct
		{
			struct vsfusbd_CDCACM_param_t param;
			struct vsf_stream_t stream_tx;
			struct vsf_stream_t stream_rx;
			uint8_t txbuff[65];
			uint8_t rxbuff[65];
			vsfsm_evt_t evt_buff[8];
		} cdc;
		struct vsfusbd_iface_t ifaces[3];
		struct vsfusbd_config_t config[1];
		struct vsfusbd_device_t device;
	} usbd;
	
	struct
	{
		struct vsfshell_t shell;
		vsfsm_evt_t evt_buff[8];
	} shell;
	
	struct vsfsm_t sm;
	struct vsftimer_timer_t usbpu_timer;
} static app =
{
	{
		2,						// uint8_t port;
		13,						// uint8_t pin;
	},							// struct usb_pullup_port_t usb_pullup;
	{
		{
			{
				1, 1,
				(struct vsfusbd_desc_filter_t *)HID_Report_Descriptors,
				dimof(app.usbd.hid.reports),
				(struct vsfusbd_HID_report_t *)&app.usbd.hid.reports,
			},					// struct vsfusbd_HID_param_t param;
			{
				{USB_HID_REPORT_INPUT, 1, NULL,
					{app.usbd.hid.report0_buffer,
						sizeof(app.usbd.hid.report0_buffer)},
					HID_on_set_get_report},
			},					// struct vsfusbd_HID_report_t reports[1];
		},						// struct hid;
		{
			{
				{
					3,			// ep_out
					3, 			// ep_in
					&app.usbd.cdc.stream_tx,
								// struct vsf_stream_t *stream_tx;
					&app.usbd.cdc.stream_rx,
								// struct vsf_stream_t *stream_rx;
				},
				{
					NULL, NULL, NULL, NULL,
				},
				{
					115200,		// bitrate
					0,			// stopbittype
					0,			// paritytype
					8			// datatype
				},
			},					// struct vsfusbd_CDCACM_param_t param;
			{
				{
					(uint8_t *)&app.usbd.cdc.txbuff,
					sizeof(app.usbd.cdc.txbuff),
				},			// struct vsf_fifo_t fifo;
			},				// struct vsf_stream_t stream_tx;
			{
				{
					(uint8_t *)&app.usbd.cdc.rxbuff,
					sizeof(app.usbd.cdc.rxbuff),
				},			// struct vsf_fifo_t fifo;
			},				// struct vsf_stream_t stream_rx;
		},						// struct cdc;
		{
			{(struct vsfusbd_class_protocol_t *)&vsfusbd_HID_class,
				(void *)&app.usbd.hid.param},
			{(struct vsfusbd_class_protocol_t *)&vsfusbd_CDCACMControl_class,
				(void *)&app.usbd.cdc.param},
			{(struct vsfusbd_class_protocol_t *)&vsfusbd_CDCACMData_class,
				(void *)&app.usbd.cdc.param,
				{{app.usbd.cdc.evt_buff, dimof(app.usbd.cdc.evt_buff)}}},
		},						// struct vsfusbd_iface_t ifaces[3];
		{
			{NULL, NULL, dimof(app.usbd.ifaces),
						(struct vsfusbd_iface_t *)app.usbd.ifaces},
		},						// struct vsfusbd_config_t config[1];
		{
			dimof(app.usbd.config),
			(struct vsfusbd_config_t *)app.usbd.config,
			(struct vsfusbd_desc_filter_t *)HID_descriptors,
			0,
			(struct interface_usbd_t *)&core_interfaces.usbd,
			0,
		},						// struct vsfusbd_device_t device;
	},							// struct usbd;
	{
		{
			&app.usbd.cdc.stream_tx,
								// struct vsf_stream_t *stream_tx;
			&app.usbd.cdc.stream_rx,
								// struct vsf_stream_t *stream_rx;
			{
				{
					app.shell.evt_buff,
					dimof(app.shell.evt_buff),
				},				// struct vsfsm_evtqueue_t evtq;
			},					// struct vsfsm_t sm;
		},						// struct vsfshell_t shell;
	},							// struct shell
	{
		{NULL, 0},				// struct vsfsm_evtqueue_t evtq;
		{app_evt_handler},		// struct vsfsm_state_t init_state;
	},							// struct vsfsm_t sm;
	{
		200,					// uint32_t interval;
		&app.sm,				// struct vsfsm_t *sm;
		APP_EVT_USBPU_TO,		// vsfsm_evt_t evt;
	},							// struct vsftimer_timer_t usbpu_timer;
};

// tickclk interrupt, simply call vsftimer_callback_int
static void app_tickclk_callback_int(void *param)
{
	vsftimer_callback_int();
}

static struct vsfsm_state_t *
app_evt_handler(struct vsfsm_t *sm, vsfsm_evt_t evt)
{
	switch (evt)
	{
	case VSFSM_EVT_INIT:
		core_interfaces.core.init(NULL);
		core_interfaces.tickclk.init();
		core_interfaces.tickclk.start();
		vsftimer_init();
		core_interfaces.tickclk.set_callback(app_tickclk_callback_int, NULL);
		
		stream_init(&app.usbd.cdc.stream_rx);
		stream_init(&app.usbd.cdc.stream_tx);
		vsfusbd_device_init(&app.usbd.device);
		vsfshell_init(&app.shell.shell);
		
		if (app.usb_pullup.port != IFS_DUMMY_PORT)
		{
			core_interfaces.gpio.init(app.usb_pullup.port);
			core_interfaces.gpio.clear(app.usb_pullup.port,
										1 << app.usb_pullup.pin);
			core_interfaces.gpio.config_pin(app.usb_pullup.port,
											app.usb_pullup.pin, GPIO_OUTPP);
		}
		app.usbd.device.drv->disconnect();
		vsftimer_register(&app.usbpu_timer);
		break;
	case APP_EVT_USBPU_TO:
		if (app.usb_pullup.port != IFS_DUMMY_PORT)
		{
			core_interfaces.gpio.set(app.usb_pullup.port,
										1 << app.usb_pullup.pin);
		}
		app.usbd.device.drv->connect();
		vsftimer_unregister(&app.usbpu_timer);
		vsfsm_remove_subsm(&vsfsm_top, sm);
		break;
	}
	return NULL;
}

int main(void)
{
	vsfsm_init(&app.sm, true);
	while (1)
	{
		vsfsm_poll();
		
		vsf_enter_critical();
		if (!vsfsm_get_event_pending())
		{
			// sleep, will also enable interrupt
			core_interfaces.core.sleep(SLEEP_WFI);
		}
		else
		{
			vsf_leave_critical();
		}
	}
}
