

#ifndef EFA_IOCTL_H
#define EFA_IOCTL_H

/* Temporary till user mode portion is able to enumerate device interfaces to get the sym links */
#define EFA_DEVICE_NAME "\\\\.\\EFADevice"

#define IOCTL_EFA(i_)   CTL_CODE( FILE_DEVICE_UNKNOWN, i_, METHOD_BUFFERED, FILE_ANY_ACCESS )

#define EFA_API_INTERFACE_VERSION 3
#define EFA_GID_SIZE 16
#define MR_READ_PERMISSION 0
#define MR_READ_WRITE_PERMISSION 1

/* Supported Queue Pair types */
typedef enum EFA_WIN_QP_TYPE {
	EFA_WIN_QP_SRD = 0,
	EFA_WIN_QP_UD = 1
}EFA_WIN_QP_TYPE;

typedef struct EFA_CREATE_QP_PARAMS {
	UINT32 SendCqIndex;
	UINT32 RecvCqIndex;
	UINT32 RqDepth;
	UINT32 SqDepth;
	UINT32 SqRingSizeInBytes;
	UINT32 RqRingSizeInBytes;
	UINT16 Pdn;
	EFA_WIN_QP_TYPE QpType;
}EFA_CREATE_QP_PARAMS, *PEFA_CREATE_QP_PARAMS;

typedef struct EFA_QP_INFO {
	PVOID RqDoorbellAddr;
	PVOID SqDoorbellAddr;
	PVOID RqAddr;
	PVOID SqAddr;
	UINT32 QpSize;
	UINT32 QpHandle;
	UINT32 QpNum;
	UINT32 SendSubCqIndex;
	UINT32 RecvSubCqIndex;
}EFA_QP_INFO, *PEFA_QP_INFO;

typedef struct EFA_DESTROY_QP_PARAMS {
	UINT32 QpHandle;
}EFA_DESTROY_QP_PARAMS, *PEFA_DESTROY_QP_PARAMS;

typedef struct EFA_CREATE_CQ_PARAMS {
	/* Number of entries per sub CQ */
	UINT16 CqDepth;
	/* Number of sub CQs - Expected to be the same value returned in EFA_DEVICE_INFO */
	UINT16 NumSubCqs;
	UINT8 EntrySizeInBytes;
}EFA_CREATE_CQ_PARAMS, *PEFA_CREATE_CQ_PARAMS;

typedef struct EFA_CQ_INFO {
	/* Address of the Completion Queue buffer - Must call IOCTL_EFA_DESTROY_CQ to free it */
	PVOID CqAddr;
	UINT16 CqIndex;
	UINT16 CqActualDepth;
}EFA_CQ_INFO, *PEFA_CQ_INFO;

typedef struct EFA_DESTROY_CQ_PARAMS {
	UINT16 CqIndex;
}EFA_DESTROY_CQ_PARAMS, *PEFA_DESTROY_CQ_PARAMS;

typedef struct EFA_CREATE_AH_PARAMS {
	/* Protection Domain Number */
	UINT16 Pdn;
	/* Destination address in network byte order */
	UINT8 DestAddr[EFA_GID_SIZE];
}EFA_CREATE_AH_PARAMS, *PEFA_CREATE_AH_PARAMS;

typedef struct EFA_AH_INFO {
	UINT16 AddressHandle;
}EFA_AH_INFO, *PEFA_AH_INFO;

typedef struct EFA_DESTROY_AH_PARAMS {
	UINT16 Pdn;
	UINT16 AddressHandle;
}EFA_DESTROY_AH_PARAMS, *PEFA_DESTROY_AH_PARAMS;

typedef struct EFA_QUERY_DEVICE_PARAMS {
	/* Version of the API interface requested. Should be EFA_API_INTERFACE_VERSION from EfaIoctl.h */
	UINT32 InterfaceVersion;
}EFA_QUERY_DEVICE_PARAMS, *PEFA_QUERY_DEVICE_PARAMS;

typedef struct EFA_DEVICE_INFO {
	/* Largest supported page size */
	UINT64 PageSizeCap;
	/* Maximum number of pages that can be registered */
	UINT64 MaxMrPages;
	/* Firmware version on the device */
	UINT32 FirmwareVersion;
	UINT32 DeviceVersion;
	/* Mask containing supported features */
	UINT32 SupportedFeatures;
	/* Maximum number of Queue Pairs supported */
	UINT32 MaxQp;
	/* Maximum entries per send queue */
	UINT32 MaxSqDepth;
	/* Maximum entries per receive queue */
	UINT32 MaxRqDepth;
	/* Maximum number of Completion Queues supported */
	UINT32 MaxCq;
	/* Maximum entries per completion queue */
	UINT32 MaxCqDepth;
	/* Maximum buffer that can be inlined with the request */
	UINT32 InlineBufSize;
	/* Maximum registrations for Memory Regions */
	UINT32 MaxMr;
	/* Maximum Protection Domains supported */
	UINT32 MaxPd;
	/* Maximum Address Handles supported */
	UINT32 MaxAh;
	/* Maximum low latency queue (Send Queue) size supported */
	UINT32 MaxLlqSize;
	UINT32 Mtu;
	/* Number of sub completion queues that the completion queue will be divided into */
	UINT16 NumSubCqs;
	/* Maximum scatter gather entries supported in send queue request */
	UINT16 MaxSqSge;
	/* Maximum scatter gather entries supported in receive queue request */
	UINT16 MaxRqSge;
	/* Network Address of the device - Not routable via normal network stack. Use only with Address Handles */
	UINT8 Addr[EFA_GID_SIZE];
}EFA_DEVICE_INFO, *PEFA_DEVICE_INFO;

typedef struct EFA_REG_MR_PARAMS {
	UINT64 MrLen;
	PVOID MrAddr;
	UINT16 Pdn;
	UINT8 Permissions;
}EFA_REG_MR_PARAMS, *PEFA_REG_MR_PARAMS;

typedef struct EFA_MR_INFO {
	UINT32 LKey;
	UINT32 RKey;
}EFA_MR_INFO, *PEFA_MR_INFO;

typedef struct EFA_DEREG_MR_PARAMS {
	UINT32 LKey;
}EFA_DEREG_MR_PARAMS, *PEFA_DEREG_MR_PARAMS;

typedef struct EFA_PD_INFO {
	UINT16 Pdn;
}EFA_PD_INFO, *PEFA_PD_INFO;

typedef struct EFA_DEALLOC_PD_PARAMS {
	UINT16 Pdn;
}EFA_DEALLOC_PD_PARAMS, *PEFA_DEALLOC_PD_PARAMS;

#define IOCTL_EFA_QUERY_DEVICE          IOCTL_EFA(0x801)
#define IOCTL_EFA_CREATE_CQ             IOCTL_EFA(0x802)
#define IOCTL_EFA_DESTROY_CQ            IOCTL_EFA(0x803)
#define IOCTL_EFA_CREATE_QP             IOCTL_EFA(0x804)
#define IOCTL_EFA_DESTROY_QP            IOCTL_EFA(0x805)
#define IOCTL_EFA_REG_MR                IOCTL_EFA(0x806)
#define IOCTL_EFA_DEREG_MR              IOCTL_EFA(0x807)
#define IOCTL_EFA_ALLOC_AH              IOCTL_EFA(0x808)
#define IOCTL_EFA_DEALLOC_AH            IOCTL_EFA(0x809)
#define IOCTL_EFA_ALLOC_PD              IOCTL_EFA(0x80a)
#define IOCTL_EFA_DEALLOC_PD            IOCTL_EFA(0x80b)

#endif
