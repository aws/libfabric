#ifndef _EFA_IF_H
#define _EFA_IF_H
#include <Windows.h>
#include "EfaIoctl.h"

struct EFA_WIN_DEVICE
{
	HANDLE Device;
	UINT32 PageSize;
};

int efa_open_device(struct EFA_WIN_DEVICE *edev);
int efa_close_device(struct EFA_WIN_DEVICE *edev);

int efa_win_create_qp(struct EFA_WIN_DEVICE *edev,
	PEFA_CREATE_QP_PARAMS createQpParams,
	PEFA_QP_INFO qpInfo);
int efa_win_destroy_qp(struct EFA_WIN_DEVICE *edev,
	PEFA_DESTROY_QP_PARAMS destroyQpParams);
int efa_win_create_cq(struct EFA_WIN_DEVICE *edev,
	PEFA_CREATE_CQ_PARAMS createCqParams,
	PEFA_CQ_INFO cqInfo);
int efa_win_destroy_cq(struct EFA_WIN_DEVICE *edev,
	PEFA_DESTROY_CQ_PARAMS destroyCqParams);
int efa_win_register_mr(struct EFA_WIN_DEVICE *edev,
	PEFA_REG_MR_PARAMS regMrParams,
	PEFA_MR_INFO mrInfo);
int efa_win_dereg_mr(struct EFA_WIN_DEVICE *edev,
	PEFA_DEREG_MR_PARAMS deregMrParams);
int efa_win_create_ah(struct EFA_WIN_DEVICE *edev,
	PEFA_CREATE_AH_PARAMS createAhParams,
	PEFA_AH_INFO ahInfo);
int efa_win_destroy_ah(struct EFA_WIN_DEVICE *edev,
	PEFA_DESTROY_AH_PARAMS destroyAhParams);
int efa_win_get_device_info(struct EFA_WIN_DEVICE *edev,
	PEFA_QUERY_DEVICE_PARAMS queryDeviceParams,
	PEFA_DEVICE_INFO deviceInfo);
int efa_win_alloc_pd(struct EFA_WIN_DEVICE *edev,
	PEFA_PD_INFO pdInfo);
int efa_win_dealloc_pd(struct EFA_WIN_DEVICE *edev,
	PEFA_DEALLOC_PD_PARAMS deallocPdParams);

#endif
