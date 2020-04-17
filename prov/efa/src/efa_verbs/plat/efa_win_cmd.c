/*
 * Copyright (c) 2018-2019 Amazon.com, Inc. or its affiliates. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "infiniband/efa_verbs.h"

#include "efa_cmd.h"
#include "efa_io_defs.h" /* entry sizes */

static struct efa_context **ctx_list = NULL;
static int dev_cnt = 0;

static inline unsigned long roundup_pow_of_two(unsigned long val)
{
	unsigned long roundup = 1;

	if (val == 1)
		return (roundup << 1);

	while (roundup < val)
		roundup <<= 1;

	return roundup;
}

int efa_device_close(struct efa_context *ctx)
{
	int err;
	struct efa_device *edev = to_efa_dev(ctx->ibv_ctx.device);

	err = efa_close_device(edev);
	pthread_mutex_destroy(&ctx->ibv_ctx.mutex);
	free(ctx);

	return err;
}

static struct efa_context *efa_device_open(struct ibv_device *device)
{
	struct efa_context *ctx;
	struct efa_device *edev = to_efa_dev(device);
	EFA_QUERY_DEVICE_PARAMS params = { 0 };
	int err;

	params.InterfaceVersion = EFA_API_INTERFACE_VERSION;

	ctx = calloc(1, sizeof(*ctx));
	if (!ctx)
		return NULL;

	err = efa_open_device(edev);
	if (err)
		goto err_free_ctx;

	// Because these attributes do not change often, we cache them in our device
	// structure so we only have to query the device once.
	err = efa_win_get_device_info(edev, &params, &edev->dev_attr);
	if (err)
		goto err_close_device;

	ctx->ibv_ctx.device = device;
	ctx->sub_cqs_per_cq = edev->dev_attr.NumSubCqs;
	ctx->max_mr_size = edev->dev_attr.MaxMr;
	ctx->inject_size = edev->dev_attr.InlineBufSize;
	ctx->max_llq_size = edev->dev_attr.MaxLlqSize;
	ctx->cqe_size = sizeof(struct efa_io_rx_cdesc);
	pthread_mutex_init(&ctx->ibv_ctx.mutex);

	return ctx;

err_close_device:
	efa_close_device(edev);
err_free_ctx:
	free(ctx);
	return NULL;
}

struct ibv_device **ibv_get_device_list(int *num_devices)
{
	struct ibv_device **deviceptr;
	struct ibv_device *ibv_dev;

	deviceptr = calloc(1, sizeof(struct ibv_device *));
	if (!deviceptr)
	{
		return NULL;
	}
	*deviceptr = calloc(1, sizeof(struct efa_device));
	if (!*deviceptr)
	{
		free(deviceptr);
		return NULL;
	}
	strncpy((*deviceptr)->dev_name, "EFA", sizeof((*deviceptr)->dev_name));
	strncpy((*deviceptr)->name, "EFA", sizeof((*deviceptr)->name));
	*num_devices = 1;

	return deviceptr;
}

int efa_device_init(void)
{
	struct ibv_device **device_list = NULL;
	int ctx_idx;
	int ret;

	device_list = ibv_get_device_list(&dev_cnt);

	ctx_list = calloc(dev_cnt, sizeof(*ctx_list));
	if (!ctx_list) {
		ret = -ENOMEM;
		goto err_free_dev_list;
	}

	for (ctx_idx = 0; ctx_idx < dev_cnt; ctx_idx++) {
		ctx_list[ctx_idx] = efa_device_open(device_list[ctx_idx]);
		if (!ctx_list[ctx_idx]) {
			ret = -ENODEV;
			goto err_close_devs;
		}
	}

	free(device_list);

	return 0;

err_close_devs:
	for (ctx_idx--; ctx_idx >= 0; ctx_idx--)
		efa_device_close(ctx_list[ctx_idx]);
	free(ctx_list);
	ctx_list = NULL;
err_free_dev_list:
	free(device_list);
	dev_cnt = 0;
	return ret;
}

void efa_device_free(void)
{
	int i;

	for (i = 0; i < dev_cnt; i++)
		efa_device_close(ctx_list[i]);

	free(ctx_list);
	ctx_list = NULL;
	dev_cnt = 0;
}

struct efa_context **efa_device_get_context_list(int *num_ctx)
{
	struct efa_context **devs = NULL;
	int i;

	devs = calloc(dev_cnt, sizeof(*devs));
	if (!devs)
		goto out;

	for (i = 0; i < dev_cnt; i++)
		devs[i] = ctx_list[i];
out:
	*num_ctx = devs ? dev_cnt : 0;
	return devs;
}

void efa_device_free_context_list(struct efa_context **list)
{
	free(list);
}

int efa_cmd_alloc_ucontext(struct ibv_device *device, struct efa_context *ctx, int cmd_fd)
{
	// The linux driver returns information on 'max_inline_data' and 'sub_cqs_per_cq' as part
	// of a alloc_ucontext response. But on Windows, our driver returns this
	// information in a query_device response, which we store in the ibv context struct
	// as part of efa_device_open. Thus this function doesn't need to do anything.
	return 0;
}

static int efa_everbs_cmd_get_ex_query_dev(struct efa_context *ctx,
	struct efa_device_attr *attr)
{
	char *fw_ver;
	struct efa_device *edev = to_efa_dev(ctx->ibv_ctx.device);
	struct ibv_device_attr *device_attr = &attr->ibv_attr;
	device_attr->max_ah = edev->dev_attr.MaxAh;
	ZeroMemory(attr, sizeof(struct efa_device_attr));

	// We query device attributes once in efa_device_open,
	// and cache them in the efa_device struct.
	// This function merely reads the cached values.

	fw_ver = (char *)&edev->dev_attr.FirmwareVersion;
	snprintf(device_attr->fw_ver, sizeof(edev->dev_attr.FirmwareVersion), "%u.%u.%u.%u",
		fw_ver[0], fw_ver[1], fw_ver[2], fw_ver[3]);
	fw_ver[sizeof(edev->dev_attr.FirmwareVersion)] = '\0';
	device_attr->hw_ver = edev->dev_attr.DeviceVersion;
	device_attr->vendor_part_id = 0xefa0;
	device_attr->vendor_id = 0x1d0f;

	device_attr->max_mr_size = edev->dev_attr.MaxMrPages * PAGE_SIZE;
	device_attr->page_size_cap = edev->dev_attr.PageSizeCap;
	device_attr->max_qp = edev->dev_attr.MaxQp;
	device_attr->max_cq = edev->dev_attr.MaxCq;
	device_attr->max_pd = edev->dev_attr.MaxPd;
	device_attr->max_mr = edev->dev_attr.MaxMr;
	device_attr->max_ah = edev->dev_attr.MaxAh;
	device_attr->max_cqe = edev->dev_attr.MaxCqDepth;
	device_attr->max_qp_wr = min(edev->dev_attr.MaxSqDepth,
		edev->dev_attr.MaxRqDepth);
	device_attr->max_sge = min(edev->dev_attr.MaxSqSge,
		edev->dev_attr.MaxRqSge);
	device_attr->page_size_cap = edev->dev_attr.PageSizeCap;
	attr->max_rq_sge = edev->dev_attr.MaxRqSge;
	attr->max_sq_sge = edev->dev_attr.MaxSqSge;
	attr->max_rq_wr = edev->dev_attr.MaxRqDepth;
	attr->max_sq_wr = edev->dev_attr.MaxSqDepth;

	return 0;
}

int efa_cmd_query_device(struct efa_context *ctx, struct efa_device_attr *attr)
{
	// Normally this function calls one of three query_device functions based on some conditions,
	// but on windows we will only ever call this one.
	return efa_everbs_cmd_get_ex_query_dev(ctx, attr);
}

int efa_cmd_query_port(struct efa_context *ctx, uint8_t port, struct ibv_port_attr *attr)
{
	// We cache these attribute in the efa_device on efa_device_open.
	// Read values here, instead of querying device.
	struct efa_device *edev = to_efa_dev(ctx->ibv_ctx.device);
	attr->lmc = 1;
	attr->state = IBV_PORT_ACTIVE;
	attr->gid_tbl_len = 1;
	attr->pkey_tbl_len = 1;
	attr->max_mtu = edev->dev_attr.Mtu;
	attr->active_mtu = edev->dev_attr.Mtu;
	attr->max_msg_sz = edev->dev_attr.Mtu;
	attr->max_vl_num = 1;

	return 0;
}

struct efa_pd *efa_cmd_alloc_pd(struct efa_context *ctx)
{
	struct efa_pd *pd = NULL;
	EFA_PD_INFO result = { 0 };
	struct efa_device *edev = to_efa_dev(ctx->ibv_ctx.device);
	int err;

	pd = calloc(1, sizeof(*pd));
	if (!pd)
		return NULL;

	err = efa_win_alloc_pd(edev, &result);
	if (err)
	{
		free(pd);
		return NULL;
	}

	pd->ibv_pd.context = &ctx->ibv_ctx;
	pd->ibv_pd.handle = result.Pdn;
	pd->context = ctx;

	return pd;
}

int efa_cmd_dealloc_pd(struct efa_pd *pd)
{
	int err;
	EFA_DEALLOC_PD_PARAMS params = { 0 };
	struct efa_device *edev = to_efa_dev(pd->ibv_pd.context->device);

	params.Pdn = pd->ibv_pd.handle;

	err = efa_win_dealloc_pd(edev, &params);
	free(pd);

	return err;
}

struct ibv_mr *efa_cmd_reg_mr(struct efa_pd *pd, void *addr,
	size_t length, int access)
{
	struct ibv_mr *mr = NULL;
	struct efa_device *edev = to_efa_dev(pd->ibv_pd.context->device);
	EFA_REG_MR_PARAMS params = { 0 };
	EFA_MR_INFO result = { 0 };
	int err;

	params.MrAddr = addr;
	params.Pdn = pd->ibv_pd.handle;
	params.MrLen = length;
	params.Permissions = access;

	mr = calloc(1, sizeof(*mr));
	if (!mr)
		return NULL;

	err = efa_win_register_mr(edev, &params, &result);
	if (err)
	{
		free(mr);
		return NULL;
	}

	mr->addr = addr;
	mr->context = &pd->context->ibv_ctx;
	mr->handle = 0;
	mr->length = length;
	mr->lkey = result.LKey;
	mr->rkey = result.RKey;
	mr->pd = &pd->ibv_pd;

	return mr;
}

int efa_cmd_dereg_mr(struct ibv_mr *mr)
{
	int err;
	struct efa_device *edev = to_efa_dev(mr->context->device);
	EFA_DEREG_MR_PARAMS params = { 0 };

	params.LKey = mr->lkey;

	err = efa_win_dereg_mr(edev, &params);
	free(mr);

	return err;
}

/* context->mutex must be held */
int efa_cmd_create_cq(struct efa_cq *cq, int cq_size, uint64_t *q_mmap_key,
	uint64_t *q_mmap_size, uint32_t *cqn)
{
	struct ibv_cq *ibv_cq = NULL;
	struct efa_context *ctx = container_of(cq->domain->ctx, struct efa_context, ibv_ctx);
	struct efa_device *edev = to_efa_dev(ctx->ibv_ctx.device);
	EFA_CREATE_CQ_PARAMS params = { 0 };
	EFA_CQ_INFO result = { 0 };
	int err;
	uint8_t *buf;
	int sub_buf_size, i;

	params.CqDepth = roundup_pow_of_two(cq_size);
	params.NumSubCqs = ctx->sub_cqs_per_cq;
	params.EntrySizeInBytes = ctx->cqe_size;

	err = efa_win_create_cq(edev, &params, &result);
	if (err)
		return err;

	ibv_cq = &cq->ibv_cq;
	ibv_cq->async_events_completed = 0;
	ibv_cq->comp_events_completed = 0;
	ibv_cq->channel = NULL;
	ibv_cq->context = &ctx->ibv_ctx;
	ibv_cq->cqe = result.CqActualDepth;
	ibv_cq->cq_context = cq;
	ibv_cq->handle = result.CqIndex;
	pthread_cond_init(&ibv_cq->cond, 0);
	pthread_mutex_init(&ibv_cq->mutex, 0);

	cq->buf = result.CqAddr;
	cq->num_sub_cqs = ctx->sub_cqs_per_cq;
	*q_mmap_size = result.CqActualDepth * ctx->sub_cqs_per_cq * ctx->cqe_size;
	*cqn = result.CqIndex;

	return 0;
}

/* context->mutex must be held */
int efa_cmd_destroy_cq(struct efa_cq *cq)
{
	int err;
	EFA_DESTROY_CQ_PARAMS params = { 0 };
	struct efa_device *edev = to_efa_dev(cq->ibv_cq.context->device);

	params.CqIndex = cq->ibv_cq.handle;

	err = efa_win_destroy_cq(edev, &params);

	pthread_cond_destroy(&cq->ibv_cq.cond);
	pthread_mutex_destroy(&cq->ibv_cq.mutex);

	return err;
}

int efa_cmd_create_qp(struct efa_qp *qp, struct efa_pd *pd, struct ibv_qp_init_attr *init_attr,
	uint32_t srd_qp, struct efa_create_qp_resp *resp)
{
	int err;
	EFA_CREATE_QP_PARAMS params = { 0 };
	EFA_QP_INFO result = { 0 };
	struct ibv_qp *ibv_qp = NULL;
	struct efa_device *edev = to_efa_dev(pd->ibv_pd.context->device);

	params.Pdn = pd->ibv_pd.handle;
	params.QpType = (init_attr->qp_type == IBV_QPT_UD) ? EFA_WIN_QP_UD : EFA_WIN_QP_SRD;
	params.SendCqIndex = init_attr->send_cq->handle;
	params.RecvCqIndex = init_attr->recv_cq->handle;
       params.SqDepth = qp->sq.wq.wqe_cnt;
       params.RqDepth = qp->rq.wq.wqe_cnt;
       params.RqRingSizeInBytes = (qp->rq.wq.desc_mask + 1) * sizeof(struct efa_io_rx_desc);
       params.SqRingSizeInBytes = (qp->sq.wq.desc_mask + 1) * sizeof(struct efa_io_tx_wqe);

	err = efa_win_create_qp(edev, &params, &result);
	if (err)
		return err;

	ibv_qp = &qp->ibv_qp;
	ibv_qp->handle = result.QpHandle;
	ibv_qp->qp_num = result.QpNum;
	ibv_qp->context = pd->ibv_pd.context;
	ibv_qp->qp_context = init_attr->qp_context;
	ibv_qp->pd = &pd->ibv_pd;
	ibv_qp->send_cq = init_attr->send_cq;
	ibv_qp->recv_cq = init_attr->recv_cq;
	ibv_qp->qp_type = params.QpType;
	ibv_qp->state = IBV_QPS_RESET;
	ibv_qp->events_completed = 0;
	pthread_cond_init(&ibv_qp->cond, 0);
	pthread_mutex_init(&ibv_qp->mutex, 0);

	qp->page_size = PAGE_SIZE;
	qp->rq.buf_size = params.RqRingSizeInBytes;
	qp->rq.buf = result.RqAddr;
	qp->rq.db = result.RqDoorbellAddr;
	qp->sq.desc_ring_mmap_size = params.SqRingSizeInBytes;
	qp->sq.desc = result.SqAddr;
	qp->sq.db = result.SqDoorbellAddr;

	resp->ibv_resp.qpn = result.QpNum;
	resp->ibv_resp.max_inline_data = init_attr->cap.max_inline_data;
	resp->ibv_resp.max_recv_sge = init_attr->cap.max_recv_sge;
	resp->ibv_resp.max_send_sge = init_attr->cap.max_send_sge;
	resp->ibv_resp.max_recv_wr = init_attr->cap.max_recv_wr;
	resp->ibv_resp.max_send_wr = init_attr->cap.max_send_wr;
	resp->efa_resp.send_sub_cq_idx = result.SendSubCqIndex;
	resp->efa_resp.recv_sub_cq_idx = result.RecvSubCqIndex;
	resp->efa_resp.rq_mmap_size = params.RqRingSizeInBytes;

	return 0;
}

int efa_cmd_destroy_qp(struct efa_qp *qp)
{
	int err;
	EFA_DESTROY_QP_PARAMS params = { 0 };
	struct ibv_qp *ibv_qp = &qp->ibv_qp;
	struct efa_device *edev = to_efa_dev(ibv_qp->context->device);

	params.QpHandle = ibv_qp->handle;

	err = efa_win_destroy_qp(edev, &params);

	pthread_cond_destroy(&ibv_qp->cond);
	pthread_mutex_destroy(&ibv_qp->mutex);

	return err;
}

int efa_cmd_query_gid(struct efa_context *ctx, uint8_t port_num,
	int index, union ibv_gid *gid)
{
	struct efa_device *edev = to_efa_dev(ctx->ibv_ctx.device);
	memcpy(&gid->raw, edev->dev_attr.Addr, EFA_GID_SIZE);  // Use cached address as gid

	return 0;
}

struct efa_ah *efa_cmd_create_ah(struct efa_pd *pd, struct ibv_ah_attr *attr)
{
	int err;
	struct efa_ah *ah = NULL;
	EFA_CREATE_AH_PARAMS params = { 0 };
	EFA_AH_INFO result = { 0 };
	struct efa_device *edev = to_efa_dev(pd->ibv_pd.context->device);

	params.Pdn = pd->ibv_pd.handle;
	memcpy(&params.DestAddr, &attr->grh.dgid, ARRAYSIZE(params.DestAddr));

	ah = calloc(1, sizeof(*ah));
	if (!ah)
		return NULL;

	err = efa_win_create_ah(edev, &params, &result);
	if (err)
		goto err_free_ah;

	ah->ibv_ah.context = &pd->context->ibv_ctx;
	ah->ibv_ah.handle = result.AddressHandle;
	ah->ibv_ah.pd = &pd->ibv_pd;
	ah->efa_address_handle = result.AddressHandle;

	return ah;

err_free_ah:
	free(ah);
	return NULL;
}

int efa_cmd_destroy_ah(struct efa_ah *ah)
{
	int err;
	EFA_DESTROY_AH_PARAMS params = { 0 };
	struct efa_device *edev = to_efa_dev(ah->ibv_ah.context->device);

	params.Pdn = ah->ibv_ah.pd->handle;
	params.AddressHandle = ah->ibv_ah.handle;

	err = efa_win_destroy_ah(edev, &params);
	free(ah);

	return err;
}
