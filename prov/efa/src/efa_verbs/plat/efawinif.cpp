#include "efa.h"
#include "efawinif.h"

int efa_open_device(struct EFA_WIN_DEVICE *device)
{
	LPCSTR deviceName = EFA_DEVICE_NAME;
	if (NULL != device)
	{
		DWORD errNum;
		//
		// Open the device
		//
		HANDLE hDevice = CreateFile(deviceName,
			GENERIC_READ | GENERIC_WRITE,
			0,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			NULL);

		if (hDevice == INVALID_HANDLE_VALUE) {

			errNum = GetLastError();

			if (!(errNum == ERROR_FILE_NOT_FOUND ||
				errNum == ERROR_PATH_NOT_FOUND)) {
				return FI_ENETDOWN;
			}
		}
		else
		{
			device->Device = hDevice;
			device->PageSize = PAGE_SIZE;
			return FI_SUCCESS;
		}
	}
	return FI_ENETDOWN;
}

int efa_close_device(struct EFA_WIN_DEVICE *device)
{
	CloseHandle(device->Device);
	device->Device = INVALID_HANDLE_VALUE;
	return FI_SUCCESS;
}

int efa_win_create_qp(struct EFA_WIN_DEVICE *edev,
	PEFA_CREATE_QP_PARAMS createQpParams,
	PEFA_QP_INFO qpInfo)
{
	DWORD bytesReturned = 0;
	bool bResult = DeviceIoControl(edev->Device,
		(DWORD)IOCTL_EFA_CREATE_QP,
		createQpParams,
		sizeof(EFA_CREATE_QP_PARAMS),
		qpInfo,
		sizeof(EFA_QP_INFO),
		&bytesReturned,
		NULL
	);

	if (!bResult || bytesReturned < sizeof(EFA_QP_INFO))
	{
		return FI_EOTHER;
	}
	return 0;
}

int efa_win_destroy_qp(struct EFA_WIN_DEVICE *edev,
	PEFA_DESTROY_QP_PARAMS destroyQpParams)
{
	bool bResult = DeviceIoControl(edev->Device,
		(DWORD)IOCTL_EFA_DESTROY_QP,
		destroyQpParams,
		sizeof(EFA_DESTROY_QP_PARAMS),
		NULL,
		0,
		NULL,
		NULL
	);

	if (!bResult)
	{
		return FI_EOTHER;
	}
	return 0;
}

int efa_win_create_cq(struct EFA_WIN_DEVICE *edev,
	PEFA_CREATE_CQ_PARAMS createCqParams,
	PEFA_CQ_INFO cqInfo)
{
	DWORD bytesReturned = 0;
	bool bResult = DeviceIoControl(edev->Device,
		(DWORD)IOCTL_EFA_CREATE_CQ,
		createCqParams,
		sizeof(PEFA_CREATE_CQ_PARAMS),
		cqInfo,
		sizeof(EFA_CQ_INFO),
		&bytesReturned,
		NULL
	);

	if (!bResult || bytesReturned < sizeof(EFA_CQ_INFO))
	{
		return FI_EOTHER;
	}
	return 0;
}

int efa_win_destroy_cq(struct EFA_WIN_DEVICE *edev,
	PEFA_DESTROY_CQ_PARAMS destroyCqParams)
{
	bool bResult = DeviceIoControl(edev->Device,
		(DWORD)IOCTL_EFA_DESTROY_CQ,
		destroyCqParams,
		sizeof(EFA_DESTROY_CQ_PARAMS),
		NULL,
		0,
		NULL,
		NULL
	);

	if (!bResult)
	{
		return FI_EOTHER;
	}
	return 0;
}

int efa_win_register_mr(struct EFA_WIN_DEVICE *edev,
	PEFA_REG_MR_PARAMS regMrParams,
	PEFA_MR_INFO mrInfo)
{
	DWORD bytesReturned = 0;
	bool bResult = DeviceIoControl(edev->Device,
		(DWORD)IOCTL_EFA_REG_MR,
		regMrParams,
		sizeof(EFA_REG_MR_PARAMS),
		mrInfo,
		sizeof(EFA_MR_INFO),
		&bytesReturned,
		NULL
	);

	if (!bResult || bytesReturned < sizeof(EFA_MR_INFO))
	{
		return FI_EOTHER;
	}
	return 0;
}

int efa_win_dereg_mr(struct EFA_WIN_DEVICE *edev,
	PEFA_DEREG_MR_PARAMS deregMrParams)
{
	bool bResult = DeviceIoControl(edev->Device,
		(DWORD)IOCTL_EFA_DEREG_MR,
		deregMrParams,
		sizeof(EFA_DEREG_MR_PARAMS),
		NULL,
		0,
		NULL,
		NULL
	);

	if (!bResult)
	{
		return FI_EOTHER;
	}
	return 0;
}

int efa_win_create_ah(struct EFA_WIN_DEVICE *edev,
	PEFA_CREATE_AH_PARAMS createAhParams,
	PEFA_AH_INFO ahInfo)
{
	DWORD bytesReturned = 0;
	bool bResult = DeviceIoControl(edev->Device,
		(DWORD)IOCTL_EFA_ALLOC_AH,
		createAhParams,
		sizeof(EFA_CREATE_AH_PARAMS),
		ahInfo,
		sizeof(EFA_AH_INFO),
		&bytesReturned,
		NULL
	);

	if (!bResult || bytesReturned < sizeof(EFA_AH_INFO))
	{
		return FI_EOTHER;
	}
	return 0;
}

int efa_win_destroy_ah(struct EFA_WIN_DEVICE *edev,
	PEFA_DESTROY_AH_PARAMS destroyAhParams)
{
	bool bResult = DeviceIoControl(edev->Device,
		(DWORD)IOCTL_EFA_DEALLOC_AH,
		destroyAhParams,
		sizeof(EFA_DESTROY_AH_PARAMS),
		NULL,
		0,
		NULL,
		NULL
	);

	if (!bResult)
	{
		return FI_EOTHER;
	}
	return 0;
}

int efa_win_get_device_info(struct EFA_WIN_DEVICE *edev,
	PEFA_QUERY_DEVICE_PARAMS queryDeviceParams,
	PEFA_DEVICE_INFO deviceInfo)
{
	DWORD bytesReturned = 0;
	bool bResult = DeviceIoControl(edev->Device,
		(DWORD)IOCTL_EFA_QUERY_DEVICE,
		queryDeviceParams,
		sizeof(EFA_QUERY_DEVICE_PARAMS),
		deviceInfo,
		sizeof(EFA_DEVICE_INFO),
		&bytesReturned,
		NULL
	);

	if (!bResult || bytesReturned < sizeof(EFA_DEVICE_INFO))
	{
		return FI_EOTHER;
	}
	return 0;
}

int efa_win_alloc_pd(struct EFA_WIN_DEVICE *edev,
	PEFA_PD_INFO pdInfo)
{
	DWORD bytesReturned = 0;
	bool bResult = DeviceIoControl(edev->Device,
		(DWORD)IOCTL_EFA_ALLOC_PD,
		NULL,
		0,
		pdInfo,
		sizeof(EFA_PD_INFO),
		&bytesReturned,
		NULL
	);

	if (!bResult || bytesReturned < sizeof(EFA_PD_INFO))
	{
		return FI_EOTHER;
	}
	return 0;
}

int efa_win_dealloc_pd(struct EFA_WIN_DEVICE *edev,
	PEFA_DEALLOC_PD_PARAMS deallocPdParams)
{
	bool bResult = DeviceIoControl(edev->Device,
		(DWORD)IOCTL_EFA_DEALLOC_PD,
		deallocPdParams,
		sizeof(EFA_DEALLOC_PD_PARAMS),
		NULL,
		0,
		NULL,
		NULL
	);

	if (!bResult)
	{
		return FI_EOTHER;
	}
	return 0;
}