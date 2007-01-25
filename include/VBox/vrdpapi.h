/** @file
 * VBox Remote Desktop Protocol:
 * Public APIs.
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#ifndef __VBox_vrdpapi_h__
#define __VBox_vrdpapi_h__

#include <VBox/cdefs.h>
#include <VBox/types.h>

#ifdef IN_RING0
# error "There are no VRDP APIs available in Ring-0 Host Context!"
#endif
#ifdef IN_GC
# error "There are no VRDP APIs available Guest Context!"
#endif


/** @defgroup grp_vrdp    VRDP
 * VirtualBox Remote Desktop Protocol (VRDP) interface that allows to use
 * the VRDP server.
 * @{
 */

/** Flags for VDRPSetFramebuffer */
#define VRDP_INTERNAL_FRAMEBUFFER (0)
#define VRDP_EXTERNAL_FRAMEBUFFER (1)

/** Default port that VRDP binds to */
#define VRDP_DEFAULT_PORT (3389)

__BEGIN_DECLS

/* Forward declaration */

#ifdef __cplusplus

class VRDPServer;
typedef class VRDPServer *HVRDPSERVER;

#else

struct VRDPServer;
typedef struct VRDPServer *HVRDPSERVER;

#endif

/**
 * Start server for the specified IConsole.
 *
 * @return VBox status code
 * @param pconsole     Pointer to IConsole instance to fetch display, mouse and keyboard interfaces.
 * @param pvrdpserver  Pointer to IVRDPServer instance to fetch configuration.
 * @param phserver     Pointer to server instance handle,
 *                     created by the function.
 */
VRDPR3DECL(int) VRDPStartServer (IConsole *pconsole, IVRDPServer *pvrdpserver, HVRDPSERVER *phserver);


/**
 * Set framebuffer which will be delivered to client.
 *
 * @param hserver      Handle of VRDP server instance.
 * @param pframebuffer Pointer to external IFramebuffer to be used.
 * @param flags        VRDP_EXTERNAL_FRAMEBUFFER - the server must use supplied
 *                         framebuffer or do not use framebuffer if pframebuffer == NULL
 *                     VRDP_INTERNAL_FRAMEBUFFER - the server must create internal
 *                         framebuffer and register it with Machine's IDisplay,
 *                         pframebuffer ignored.
 */
VRDPR3DECL(int) VRDPSetFramebuffer (HVRDPSERVER hserver, IFramebuffer *pframebuffer, uint32_t flags);

/**
 * Shutdown server.
 *
 * @param hserver Handle of VRDP server instance to be stopped.
 */
VRDPR3DECL(void) VRDPShutdownServer (HVRDPSERVER hserver);


/**
 * Send framebuffer bitmap to client.
 *
 * @param hserver Handle of VRDP server instance.
 * @param x       top left horizontal coordinate in framebuffer.
 * @param y       top left vertical coordinate in framebuffer.
 * @param w       width of rectangle.
 * @param h       height of rectangle.
 */
VRDPR3DECL(void) VRDPSendUpdateBitmap (HVRDPSERVER hserver, unsigned x, unsigned y, unsigned w, unsigned h);

/**
 * Inform client that display was resized.
 * New width and height are taken from the current framebuffer.
 *
 * @param hserver Handle of VRDP server instance.
 */
VRDPR3DECL(void) VRDPSendResize (HVRDPSERVER hserver);

/**
 * Send an update in accelerated mode.
 *
 * @param hserver   Handle of VRDP server instance.
 * @param pvUpdate  The update information. Actually pointer to VBoxGuest.h::VBVACMDHDR structure with extra data.
 * @param cbUpdate  Size of the update data.
 */
VRDPR3DECL(void) VRDPSendUpdate (HVRDPSERVER hserver, void *pvUpdate, uint32_t cbUpdate);

/** @todo comment the structure. */
typedef struct _VRDPCOLORPOINTER
{
    uint16_t xHot;
    uint16_t yHot;
    uint16_t width;
    uint16_t height;
    uint16_t masklen;
    uint16_t datalen;
    uint32_t vrdpInternal;
} VRDPCOLORPOINTER;

typedef VRDPCOLORPOINTER *PVRDPCOLORPOINTER;

/**
 * Set mouse pointer shape.
 *
 * @param hserver Handle of VRDP server instance.
 * @param porder  The pointer shape information.
 */
VRDPR3DECL(void) VRDPSendColorPointer (HVRDPSERVER hserver, PVRDPCOLORPOINTER pdata);

/**
 * Hide mouse pointer.
 *
 * @param hserver Handle of VRDP server instance.
 */
VRDPR3DECL(void) VRDPSendHidePointer (HVRDPSERVER hserver);

/** Audio format information packed in a 32 bit value. */
typedef uint32_t VRDPAUDIOFORMAT;

/** Constructs 32 bit value for given frequency, number of channel and bits per sample. */
#define VRDP_AUDIO_FMT_MAKE(freq, c, bps, s) ((((s) & 0x1) << 28) + (((bps) & 0xFF) << 20) + (((c) & 0xF) << 16) + ((freq) & 0xFFFF))

/** Decode frequency. */
#define VRDP_AUDIO_FMT_SAMPLE_FREQ(a) ((a) & 0xFFFF)
/** Decode number of channels. */
#define VRDP_AUDIO_FMT_CHANNELS(a) (((a) >> 16) & 0xF)
/** Decode number signess. */
#define VRDP_AUDIO_FMT_SIGNED(a) (((a) >> 28) & 0x1)
/** Decode number of bits per sample. */
#define VRDP_AUDIO_FMT_BITS_PER_SAMPLE(a) (((a) >> 20) & 0xFF)
/** Decode number of bytes per sample. */
#define VRDP_AUDIO_FMT_BYTES_PER_SAMPLE(a) ((VRDP_AUDIO_FMT_BITS_PER_SAMPLE(a) + 7) / 8)

/**
 * Queues the samples to be sent to client.
 *
 * @param hserver    Handle of VRDP server instance.
 * @param pvSamples  Address of samples to be sent.
 * @param cSamples   Number of samples.
 * @param format     Encoded audio format for these samples.
 */
VRDPR3DECL(void) VRDPSendAudioSamples (HVRDPSERVER hserver, void *pvSamples, uint32_t cSamples, VRDPAUDIOFORMAT format);

/**
 * Sets sound volume on client.
 *
 * @param hserver    Handle of VRDP server instance.
 * @param left       0..0xFFFF volume level for left channel.
 * @param right      0..0xFFFF volume level for right channel.
 */
VRDPR3DECL(void) VRDPSendAudioVolume (HVRDPSERVER hserver, uint16_t left, uint16_t right);


/*
 * Remote USB backend protocol.
 */

/* The version of Remote USB Protocol. */
#define VRDP_USB_VERSION (1)

/** USB backend operations. */
#define VRDP_USB_REQ_OPEN              (0)
#define VRDP_USB_REQ_CLOSE             (1)
#define VRDP_USB_REQ_RESET             (2)
#define VRDP_USB_REQ_SET_CONFIG        (3)
#define VRDP_USB_REQ_CLAIM_INTERFACE   (4)
#define VRDP_USB_REQ_RELEASE_INTERFACE (5)
#define VRDP_USB_REQ_INTERFACE_SETTING (6)
#define VRDP_USB_REQ_QUEUE_URB         (7)
#define VRDP_USB_REQ_REAP_URB          (8)
#define VRDP_USB_REQ_CLEAR_HALTED_EP   (9)
#define VRDP_USB_REQ_CANCEL_URB        (10)

/** USB service operations. */
#define VRDP_USB_REQ_DEVICE_LIST       (11)
#define VRDP_USB_REQ_NEGOTIATE         (12)

/** An operation completion status is a byte. */
typedef uint8_t VRDPUSBSTATUS;

/** USB device identifier is an 32 bit value. */
typedef uint32_t VRDPUSBDEVID;

/** Status codes. */
#define VRDP_USB_STATUS_SUCCESS        ((VRDPUSBSTATUS)0)
#define VRDP_USB_STATUS_ACCESS_DENIED  ((VRDPUSBSTATUS)1)
#define VRDP_USB_STATUS_DEVICE_REMOVED ((VRDPUSBSTATUS)2)

/*
 * Data structures to use with VRDPSendUSBRequest.
 * The *RET* structures always represent the layout of VRDP data.
 * The *PARM* structures normally the same as VRDP layout.
 * However the VRDP_USB_REQ_QUEUE_URB_PARM has a pointer to
 * URB data in place where actual data will be in VRDP layout.
 *
 * Since replies (*RET*) are asynchronous, the 'success'
 * replies are not required for operations which return
 * only the status code (VRDPUSBREQRETHDR only):
 *  VRDP_USB_REQ_OPEN
 *  VRDP_USB_REQ_RESET
 *  VRDP_USB_REQ_SET_CONFIG
 *  VRDP_USB_REQ_CLAIM_INTERFACE
 *  VRDP_USB_REQ_RELEASE_INTERFACE
 *  VRDP_USB_REQ_INTERFACE_SETTING
 *  VRDP_USB_REQ_CLEAR_HALTED_EP
 *
 */

/* VRDP layout has no aligments. */
#pragma pack(1)

/* Common header for all VRDP USB packets. After the reply hdr follows *PARM* or *RET* data. */
typedef struct _VRDPUSBPKTHDR
{
    /* Total length of the reply NOT including the 'length' field. */
    uint32_t length;
    /* The operation code for which the reply was sent by the client. */
    uint8_t code;
} VRDPUSBPKTHDR;

/* Common header for all return structures. */
typedef struct _VRDPUSBREQRETHDR
{
    /* Device status. */
    VRDPUSBSTATUS status;
    /* Device id. */
    VRDPUSBDEVID id;
} VRDPUSBREQRETHDR;


/* VRDP_USB_REQ_OPEN
 */
typedef struct _VRDP_USB_REQ_OPEN_PARM
{
    uint8_t code;
    VRDPUSBDEVID id;
} VRDP_USB_REQ_OPEN_PARM;

typedef struct _VRDP_USB_REQ_OPEN_RET
{
    VRDPUSBREQRETHDR hdr;
} VRDP_USB_REQ_OPEN_RET;


/* VRDP_USB_REQ_CLOSE
 */
typedef struct _VRDP_USB_REQ_CLOSE_PARM
{
    uint8_t code;
    VRDPUSBDEVID id;
} VRDP_USB_REQ_CLOSE_PARM;

/* The close request has no returned data. */


/* VRDP_USB_REQ_RESET
 */
typedef struct _VRDP_USB_REQ_RESET_PARM
{
    uint8_t code;
    VRDPUSBDEVID id;
} VRDP_USB_REQ_RESET_PARM;

typedef struct _VRDP_USB_REQ_RESET_RET
{
    VRDPUSBREQRETHDR hdr;
} VRDP_USB_REQ_RESET_RET;


/* VRDP_USB_REQ_SET_CONFIG
 */
typedef struct _VRDP_USB_REQ_SET_CONFIG_PARM
{
    uint8_t code;
    VRDPUSBDEVID id;
    uint8_t configuration;
} VRDP_USB_REQ_SET_CONFIG_PARM;

typedef struct _VRDP_USB_REQ_SET_CONFIG_RET
{
    VRDPUSBREQRETHDR hdr;
} VRDP_USB_REQ_SET_CONFIG_RET;


/* VRDP_USB_REQ_CLAIM_INTERFACE
 */
typedef struct _VRDP_USB_REQ_CLAIM_INTERFACE_PARM
{
    uint8_t code;
    VRDPUSBDEVID id;
    uint8_t iface;
} VRDP_USB_REQ_CLAIM_INTERFACE_PARM;

typedef struct _VRDP_USB_REQ_CLAIM_INTERFACE_RET
{
    VRDPUSBREQRETHDR hdr;
} VRDP_USB_REQ_CLAIM_INTERFACE_RET;


/* VRDP_USB_REQ_RELEASE_INTERFACE
 */
typedef struct _VRDP_USB_REQ_RELEASE_INTERFACE_PARM
{
    uint8_t code;
    VRDPUSBDEVID id;
    uint8_t iface;
} VRDP_USB_REQ_RELEASE_INTERFACE_PARM;

typedef struct _VRDP_USB_REQ_RELEASE_INTERFACE_RET
{
    VRDPUSBREQRETHDR hdr;
} VRDP_USB_REQ_RELEASE_INTERFACE_RET;


/* VRDP_USB_REQ_INTERFACE_SETTING
 */
typedef struct _VRDP_USB_REQ_INTERFACE_SETTING_PARM
{
    uint8_t code;
    VRDPUSBDEVID id;
    uint8_t iface;
    uint8_t setting;
} VRDP_USB_REQ_INTERFACE_SETTING_PARM;

typedef struct _VRDP_USB_REQ_INTERFACE_SETTING_RET
{
    VRDPUSBREQRETHDR hdr;
} VRDP_USB_REQ_INTERFACE_SETTING_RET;


/* VRDP_USB_REQ_QUEUE_URB
 */

#define VRDP_USB_TRANSFER_TYPE_CTRL (0)
#define VRDP_USB_TRANSFER_TYPE_ISOC (1)
#define VRDP_USB_TRANSFER_TYPE_BULK (2)
#define VRDP_USB_TRANSFER_TYPE_INTR (3)
#define VRDP_USB_TRANSFER_TYPE_MSG  (4)

#define VRDP_USB_DIRECTION_SETUP (0)
#define VRDP_USB_DIRECTION_IN    (1)
#define VRDP_USB_DIRECTION_OUT   (2)

typedef struct _VRDP_USB_REQ_QUEUE_URB_PARM
{
    uint8_t code;
    VRDPUSBDEVID id;
    uint32_t handle;    /* Distinguishes that particular URB. Later used in CancelURB and returned by ReapURB */
    uint8_t type;
    uint8_t ep;
    uint8_t direction;
    uint32_t urblen;    /* Length of the URB. */
    uint32_t datalen;   /* Length of the data. */
    void *data;         /* In RDP layout the data follow. */
} VRDP_USB_REQ_QUEUE_URB_PARM;

/* The queue URB has no explicit return. The reap URB reply will be
 * eventually the indirect result.
 */


/* VRDP_USB_REQ_REAP_URB
 * Notificationg from server to client that server expects an URB
 * from any device.
 * Only sent if negotiated URB return method is polling.
 * Normally, the client will send URBs back as soon as they are ready.
 */
typedef struct _VRDP_USB_REQ_REAP_URB_PARM
{
    uint8_t code;
} VRDP_USB_REQ_REAP_URB_PARM;


#define VRDP_USB_XFER_OK    (0)
#define VRDP_USB_XFER_STALL (1)
#define VRDP_USB_XFER_DNR   (2)
#define VRDP_USB_XFER_CRC   (3)

#define VRDP_USB_REAP_FLAG_CONTINUED (0x0)
#define VRDP_USB_REAP_FLAG_LAST      (0x1)

#define VRDP_USB_REAP_VALID_FLAGS    (VRDP_USB_REAP_FLAG_LAST)

typedef struct _VRDPUSBREQREAPURBBODY
{
    VRDPUSBDEVID     id;        /* From which device the URB arrives. */
    uint8_t          flags;     /* VRDP_USB_REAP_FLAG_* */
    uint8_t          error;     /* VRDP_USB_XFER_* */
    uint32_t         handle;    /* Handle of returned URB. Not 0. */
    uint32_t         len;       /* Length of data actually transferred. */
    /* Data follow. */
} VRDPUSBREQREAPURBBODY;

typedef struct _VRDP_USB_REQ_REAP_URB_RET
{
    /* The REAP URB has no header, only completed URBs are returned. */
    VRDPUSBREQREAPURBBODY body;
    /* Another body may follow, depending on flags. */
} VRDP_USB_REQ_REAP_URB_RET;


/* VRDP_USB_REQ_CLEAR_HALTED_EP
 */
typedef struct _VRDP_USB_REQ_CLEAR_HALTED_EP_PARM
{
    uint8_t code;
    VRDPUSBDEVID id;
    uint8_t ep;
} VRDP_USB_REQ_CLEAR_HALTED_EP_PARM;

typedef struct _VRDP_USB_REQ_CLEAR_HALTED_EP_RET
{
    VRDPUSBREQRETHDR hdr;
} VRDP_USB_REQ_CLEAR_HALTED_EP_RET;


/* VRDP_USB_REQ_CANCEL_URB
 */
typedef struct _VRDP_USB_REQ_CANCEL_URB_PARM
{
    uint8_t code;
    VRDPUSBDEVID id;
    uint32_t handle;
} VRDP_USB_REQ_CANCEL_URB_PARM;

/* The cancel URB request has no return. */


/* VRDP_USB_REQ_DEVICE_LIST
 *
 * Server polls USB devices on client by sending this request
 * periodically. Client sends back a list of all devices
 * connected to it. Each device is assigned with an identifier,
 * that is used to distinguish the particular device.
 */
typedef struct _VRDP_USB_REQ_DEVICE_LIST_PARM
{
    uint8_t code;
} VRDP_USB_REQ_DEVICE_LIST_PARM;

/* Data is a list of the following variable length structures. */
typedef struct _VRDPUSBDEVICEDESC
{
    /* Offset of the next structure. 0 if last. */
    uint16_t oNext;

    /* Identifier of the device assigned by client. */
    VRDPUSBDEVID id;

    /** USB version number. */
    uint16_t        bcdUSB;
    /** Device class. */
    uint8_t         bDeviceClass;
    /** Device subclass. */
    uint8_t         bDeviceSubClass;
    /** Device protocol */
    uint8_t         bDeviceProtocol;
    /** Vendor ID. */
    uint16_t        idVendor;
    /** Product ID. */
    uint16_t        idProduct;
    /** Revision, integer part. */
    uint16_t        bcdRev;
    /** Manufacturer string. */
    uint16_t        oManufacturer;
    /** Product string. */
    uint16_t        oProduct;
    /** Serial number string. */
    uint16_t        oSerialNumber;
    /** Physical USB port the device is connected to. */
    uint16_t        idPort;

} VRDPUSBDEVICEDESC;

typedef struct _VRDP_USB_REQ_DEVICE_LIST_RET
{
    VRDPUSBDEVICEDESC body;
    /* Other devices may follow.
     * The list ends with (uint16_t)0,
     * which means that an empty list consists of 2 zero bytes.
     */
} VRDP_USB_REQ_DEVICE_LIST_RET;

typedef struct _VRDPUSBREQNEGOTIATEPARM
{
    uint8_t code;

    /* Remote USB Protocol version. */
    uint32_t version;

} VRDPUSBREQNEGOTIATEPARM;

#define VRDP_USB_CAPS_FLAG_ASYNC    (0x0)
#define VRDP_USB_CAPS_FLAG_POLL     (0x1)

#define VRDP_USB_CAPS_VALID_FLAGS   (VRDP_USB_CAPS_FLAG_POLL)

typedef struct _VRDPUSBREQNEGOTIATERET
{
    uint8_t flags;
} VRDPUSBREQNEGOTIATERET;

#pragma pack()


#define VRDP_CLIPBOARD_FORMAT_INVALID      (0xFFFFFFFF)
#define VRDP_CLIPBOARD_FORMAT_UNICODE_TEXT (0)
#define VRDP_CLIPBOARD_FORMAT_BITMAP       (1)

VRDPR3DECL(void) VRDPSendClipboardData (HVRDPSERVER hserver, uint32_t u32Format, void *pvData, uint32_t cbData);

#ifdef VRDP_MC
/**
 * Sends a USB request.
 *
 * @param hserver      Handle of VRDP server instance.
 * @param u32ClientId  An identifier that allows the server to find the corresponding client.
 *                     The identifier is always passed by the server as a parameter
 *                     of the FNVRDPUSBCALLBACK. Note that the value is the same as
 *                     in the VRDPSERVERCALLBACK functions.
 * @param pvParm       Function specific parameters buffer.
 * @param cbParm       Size of the buffer.
 */
VRDPR3DECL(void) VRDPSendUSBRequest (HVRDPSERVER hserver,
                                     uint32_t u32ClientId,
                                     void *pvParm,
                                     uint32_t cbRarm);


/**
 * Called by the server when a reply is received from a client.
 *
 * @param pvCallback  Callback specific value returned by VRDPSERVERCALLBACK::pfnInterceptUSB.
 * @param u32ClientId Identifies the client that sent the reply.
 * @param u8Code      The operation code VRDP_USB_REQ_*.
 * @param pvRet       Points to data received from the client.
 * @param cbRet       Size of the data in bytes.
 *
 * @return VBox error code.
 */
typedef DECLCALLBACK(int) FNVRDPUSBCALLBACK (void *pvCallback,
                                             uint32_t u32ClientId,
                                             uint8_t u8Code,
                                             const void *pvRet,
                                             uint32_t cbRet);
                                             
typedef FNVRDPUSBCALLBACK *PFNVRDPUSBCALLBACK;

/**
 * Called by the server when a clipboard data is received from a client.
 *
 * @param pvCallback  Callback specific value returned by VRDPSERVERCALLBACK::pfnInterceptClipboard.
 * @param u32ClientId Identifies the client that sent the reply.
 * @param u32Format   The format of data.
 * @param pvData      Points to data received from the client.
 * @param cbData      Size of the data in bytes.
 *
 * @return VBox error code.
 */
typedef DECLCALLBACK(int) FNVRDPCLIPBOARDCALLBACK (void *pvCallback,
                                                   uint32_t u32ClientId,
                                                   uint32_t u32Format,
                                                   const void *pvRet,
                                                   uint32_t cbRet);
                                             
typedef FNVRDPCLIPBOARDCALLBACK *PFNVRDPCLIPBOARDCALLBACK;

#define VRDP_CLIENT_INTERCEPT_AUDIO     (0x1)
#define VRDP_CLIENT_INTERCEPT_USB       (0x2)
#define VRDP_CLIENT_INTERCEPT_CLIPBOARD (0x4)

typedef struct _VRDPSERVERCALLBACK
{
    /* A client is logging in.
     *
     * @param pvUser       The callback specific pointer.
     * @param u32ClientId  An unique client identifier generated by the server.
     * @param pszUser      The username.
     * @param pszPassword  The password.
     * @param pszDomain    The domain.
     *
     * @return VBox error code.
     */
    DECLCALLBACKMEMBER(int, pfnClientLogon) (void *pvUser,
                                             uint32_t u32ClientId,
                                             const char *pszUser,
                                             const char *pszPassword,
                                             const char *pszDomain);
    /* The client has connected.
     *
     * @param pvUser       The callback specific pointer.
     * @param u32ClientId  An unique client identifier generated by the server.
     */
    DECLCALLBACKMEMBER(void, pfnClientConnect) (void *pvUser,
                                                uint32_t u32ClientId);
    /* The client has been disconnected.
     *
     * @param pvUser          The callback specific pointer.
     * @param u32ClientId     An unique client identifier generated by the server.
     * @param fu32Intercepted What was intercepted by the client (VRDP_CLIENT_INTERCEPT_*).
     */
    DECLCALLBACKMEMBER(void, pfnClientDisconnect) (void *pvUser,
                                                   uint32_t u32ClientId,
                                                   uint32_t fu32Intercepted);
    /* The client supports audio channel.
     */
    DECLCALLBACKMEMBER(void, pfnInterceptAudio) (void *pvUser,
                                                 uint32_t u32ClientId);
    /* The client supports USB channel.
     */
    DECLCALLBACKMEMBER(void, pfnInterceptUSB) (void *pvUser,
                                               uint32_t u32ClientId,
                                               PFNVRDPUSBCALLBACK *ppfn,
                                               void **ppv);
    /* The client supports clipboard channel.
     */
    DECLCALLBACKMEMBER(void, pfnInterceptClipboard) (void *pvUser,
                                                     uint32_t u32ClientId,
                                                     PFNVRDPCLIPBOARDCALLBACK *ppfn,
                                                     void **ppv);
} VRDPSERVERCALLBACK;
#else
/**
 * Sends a USB request.
 *
 * @param hserver    Handle of VRDP server instance.
 * @param pvParm     Function specific parameters buffer.
 * @param cbParm     Size of the buffer.
 * @param pvRet      Function specific returned data buffer.
 * @param cbRet      Size of the buffer.
 * @param prc        VBox return code.
 */
VRDPR3DECL(void) VRDPSendUSBRequest (HVRDPSERVER hserver, void *pvParm, uint32_t cbRarm);


typedef DECLCALLBACK(int) FNVRDPUSBCALLBACK (void *pv, uint8_t code, void *pvRet, uint32_t cbRet);
typedef FNVRDPUSBCALLBACK *PFNVRDPUSBCALLBACK;

typedef DECLCALLBACK(int) FNVRDPCLIPBOARDCALLBACK (void *pv, uint32_t u32Format, void *pvData, uint32_t cbData);
typedef FNVRDPCLIPBOARDCALLBACK *PFNVRDPCLIPBOARDCALLBACK;

typedef struct _VRDPSERVERCALLBACK
{
    DECLCALLBACKMEMBER(int, pfnClientLogon) (void *pvUser, const char *pszUser, const char *pszPassword, const char *pszDomain);
    DECLCALLBACKMEMBER(void, pfnClientConnect) (void *pvUser, uint32_t fu32SupportedOrders);
    DECLCALLBACKMEMBER(void, pfnClientDisconnect) (void *pvUser);
    DECLCALLBACKMEMBER(void, pfnInterceptAudio) (void *pvUser, bool fKeepHostAudio);
    DECLCALLBACKMEMBER(void, pfnInterceptUSB) (void *pvUser, PFNVRDPUSBCALLBACK *ppfn, void **ppv);
    DECLCALLBACKMEMBER(void, pfnInterceptClipboard) (void *pvUser, PFNVRDPCLIPBOARDCALLBACK *ppfn, void **ppv);
} VRDPSERVERCALLBACK;
#endif /* VRDP_MC */

/**
 * Set a callback pointers table that will be called by the server in certain situations.
 *
 * @param hserver      Handle of VRDP server instance.
 * @param pCallback    Pointer to VRDPSERVERCALLBACK structure with function pointers.
 * @param pvUser       An pointer to be passed to the callback functions.
 */
VRDPR3DECL(void) VRDPSetCallback (HVRDPSERVER hserver, VRDPSERVERCALLBACK *pCallback, void *pvUser);

/** Indexes of information values. */

/** Whether a client is connected at the moment.
 * uint32_t
 */
#define VRDP_QI_ACTIVE                 (0)

/** How many times a client connected up to current moment.
 * uint32_t
 */
#define VRDP_QI_NUMBER_OF_CLIENTS      (1)

/** When last connection was established.
 * int64_t time in milliseconds since 1970-01-01 00:00:00 UTC
 */
#define VRDP_QI_BEGIN_TIME             (2)

/** When last connection was terminated or current time if connection still active.
 * int64_t time in milliseconds since 1970-01-01 00:00:00 UTC
 */
#define VRDP_QI_END_TIME               (3)

/** How many bytes were sent in last (current) connection.
 * uint64_t
 */
#define VRDP_QI_BYTES_SENT             (4)

/** How many bytes were sent in all connections.
 * uint64_t
 */
#define VRDP_QI_BYTES_SENT_TOTAL       (5)

/** How many bytes were received in last (current) connection.
 * uint64_t
 */
#define VRDP_QI_BYTES_RECEIVED         (6)

/** How many bytes were received in all connections.
 * uint64_t
 */
#define VRDP_QI_BYTES_RECEIVED_TOTAL   (7)

/** Login user name supplied by the client.
 * UTF8 nul terminated string.
 */
#define VRDP_QI_USER                   (8)

/** Login domain supplied by the client.
 * UTF8 nul terminated string.
 */
#define VRDP_QI_DOMAIN                 (9)

/** The client name supplied by the client.
 * UTF8 nul terminated string.
 */
#define VRDP_QI_CLIENT_NAME            (10)

/** IP address of the client.
 * UTF8 nul terminated string.
 */
#define VRDP_QI_CLIENT_IP              (11)

/** The client software version number.
 * uint32_t.
 */
#define VRDP_QI_CLIENT_VERSION         (12)

/** Public key exchange method used when connection was established.
 *  Values: 0 - RDP4 public key exchange scheme.
 *          1 - X509 sertificates were sent to client.
 * uint32_t.
 */
#define VRDP_QI_ENCRYPTION_STYLE       (13)

/**
 * Query various information from the VRDP server.
 *
 * @param index     VRDP_QI_* identifier of information to be returned.
 * @param pvBuffer  Address of memory buffer to which the information must be written.
 * @param cbBuffer  Size of the memory buffer in bytes.
 * @param pcbOut    Size in bytes of returned information value.
 *
 * @remark The caller must check the *pcbOut. 0 there means no information was returned.
 *         A value greater than cbBuffer means that information is too big to fit in the
 *         buffer, in that case no information was placed to the buffer.
 */
VRDPR3DECL(void) VRDPQueryInfo (HVRDPSERVER hserver, uint32_t index, void *pvBuffer, uint32_t cbBuffer, uint32_t *pcbOut);

__END_DECLS

/** @} */

#endif /* __VBox_vrdpapi_h__ */

