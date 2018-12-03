/*
 * Copyright (c) 2019 Amazon.com, Inc. or its affiliates.
 * All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * BSD license below:
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

#include <rdma/fi_errno.h>

#include <ofi_prov.h>
#include "rxr.h"

struct rxr_env rxr_env = {
	.rx_window_size	= RXR_DEF_MAX_RX_WINDOW,
	.tx_queue_size = 0,
	.recvwin_size = RXR_RECVWIN_SIZE,
	.cq_size = RXR_DEF_CQ_SIZE,
	.inline_mr_enable = 0,
	.max_memcpy_size = 4096,
	.mtu_size = 0,
	.tx_size = 0,
	.rx_size = 0,
	.rx_copy_unexp = 1,
	.rx_copy_ooo = 1,
	.max_timeout = RXR_DEF_RNR_MAX_TIMEOUT,
	.timeout_interval = 0, /* 0 is random timeout */
};

static void rxr_init_env(void)
{
	fi_param_get_int(&rxr_prov, "rx_window_size", &rxr_env.rx_window_size);
	fi_param_get_int(&rxr_prov, "tx_queue_size", &rxr_env.tx_queue_size);
	fi_param_get_int(&rxr_prov, "recvwin_size", &rxr_env.recvwin_size);
	fi_param_get_int(&rxr_prov, "cq_size", &rxr_env.cq_size);
	fi_param_get_bool(&rxr_prov, "inline_mr_enable",
			  &rxr_env.inline_mr_enable);
	fi_param_get_size_t(&rxr_prov, "max_memcpy_size",
			    &rxr_env.max_memcpy_size);
	fi_param_get_size_t(&rxr_prov, "mtu_size",
			    &rxr_env.mtu_size);
	fi_param_get_size_t(&rxr_prov, "tx_size", &rxr_env.tx_size);
	fi_param_get_size_t(&rxr_prov, "rx_size", &rxr_env.rx_size);
	fi_param_get_bool(&rxr_prov, "rx_copy_unexp",
			  &rxr_env.rx_copy_unexp);
	fi_param_get_bool(&rxr_prov, "rx_copy_ooo",
			  &rxr_env.rx_copy_ooo);
	fi_param_get_int(&rxr_prov, "max_timeout", &rxr_env.max_timeout);
	fi_param_get_int(&rxr_prov, "timeout_interval",
			 &rxr_env.timeout_interval);
}

void rxr_info_to_core_mr_modes(uint32_t version,
			       const struct fi_info *hints,
			       struct fi_info *core_info)
{
	if (hints && hints->domain_attr &&
	    (hints->domain_attr->mr_mode & (FI_MR_SCALABLE | FI_MR_BASIC))) {
		core_info->mode = FI_LOCAL_MR | FI_MR_ALLOCATED;
		core_info->domain_attr->mr_mode = hints->domain_attr->mr_mode;
	} else if (FI_VERSION_LT(version, FI_VERSION(1, 5))) {
		core_info->mode |= FI_LOCAL_MR | FI_MR_ALLOCATED;
		core_info->domain_attr->mr_mode = FI_MR_UNSPEC;
	} else {
		core_info->domain_attr->mr_mode |=
			FI_MR_LOCAL | FI_MR_ALLOCATED;
		if (!hints)
			core_info->domain_attr->mr_mode |= OFI_MR_BASIC_MAP;
		else if (hints->domain_attr)
			core_info->domain_attr->mr_mode |=
				hints->domain_attr->mr_mode & OFI_MR_BASIC_MAP;
	}
}

int rxr_info_to_core(uint32_t version,
		     const struct fi_info *rxr_info,
		     struct fi_info *core_info)
{
	rxr_info_to_core_mr_modes(version, rxr_info, core_info);
	core_info->caps = FI_MSG | FI_SOURCE;
	core_info->ep_attr->type = FI_EP_RDM;
	core_info->tx_attr->op_flags = FI_TRANSMIT_COMPLETE;
	return 0;
}

/* Pass tx/rx attr that user specifies down to core provider */
void rxr_reset_rx_tx_to_core(const struct fi_info *user_info,
			     struct fi_info *core_info)
{
	/* rx attr */
	core_info->rx_attr->total_buffered_recv =
		user_info->rx_attr->total_buffered_recv < core_info->rx_attr->total_buffered_recv ?
		user_info->rx_attr->total_buffered_recv : core_info->rx_attr->total_buffered_recv;
	core_info->rx_attr->size =
		user_info->rx_attr->size < core_info->rx_attr->size ?
		user_info->rx_attr->size : core_info->rx_attr->size;
	core_info->rx_attr->iov_limit =
		user_info->rx_attr->iov_limit < core_info->rx_attr->iov_limit ?
		user_info->rx_attr->iov_limit : core_info->rx_attr->iov_limit;
	/* tx attr */
	core_info->tx_attr->inject_size =
		user_info->tx_attr->inject_size < core_info->tx_attr->inject_size ?
		user_info->tx_attr->inject_size : core_info->tx_attr->inject_size;
	core_info->tx_attr->size =
		user_info->tx_attr->size < core_info->tx_attr->size ?
		user_info->tx_attr->size : core_info->tx_attr->size;
	core_info->tx_attr->iov_limit =
		user_info->tx_attr->iov_limit < core_info->tx_attr->iov_limit ?
		user_info->tx_attr->iov_limit : core_info->tx_attr->iov_limit;
}

void rxr_set_rx_tx_size(struct fi_info *info,
			const struct fi_info *core_info)
{
	if (rxr_env.tx_size > 0)
		info->tx_attr->size = rxr_env.tx_size;
	else
		info->tx_attr->size = core_info->tx_attr->size;

	if (rxr_env.rx_size > 0)
		info->rx_attr->size = rxr_env.rx_size;
	else
		info->rx_attr->size = core_info->rx_attr->size;
}

int rxr_info_to_rxr(uint32_t version,
		    const struct fi_info *core_info,
		    struct fi_info *info)
{
	info->caps = rxr_info.caps;
	info->mode = rxr_info.mode;

	*info->tx_attr = *rxr_info.tx_attr;
	*info->rx_attr = *rxr_info.rx_attr;
	*info->ep_attr = *rxr_info.ep_attr;
	*info->domain_attr = *rxr_info.domain_attr;

	info->tx_attr->inject_size =
		core_info->tx_attr->inject_size > RXR_CTRL_HDR_SIZE_NO_CQ ?
		core_info->tx_attr->inject_size - RXR_CTRL_HDR_SIZE_NO_CQ
		: 0;
	rxr_info.tx_attr->inject_size = info->tx_attr->inject_size;

	info->addr_format = core_info->addr_format;
	info->domain_attr->ep_cnt = core_info->domain_attr->ep_cnt;
	info->domain_attr->cq_cnt = core_info->domain_attr->cq_cnt;
	info->domain_attr->mr_key_size = core_info->domain_attr->mr_key_size;

	rxr_set_rx_tx_size(info, core_info);
	return 0;
}

static int rxr_getinfo(uint32_t version, const char *node,
		       const char *service, uint64_t flags,
		       const struct fi_info *hints, struct fi_info **info)
{
	if (hints && hints->tx_attr &&
	    (hints->tx_attr->op_flags & FI_DELIVERY_COMPLETE)) {
		FI_WARN(&rxr_prov, FI_LOG_CORE,
			"FI_DELIVERY_COMPLETE unsupported\n");
		return -ENODATA;
	}

	return ofix_getinfo(version, node, service, flags, &rxr_util_prov,
			    hints, rxr_info_to_core, rxr_info_to_rxr, info);
}

static void rxr_fini(void)
{
	/* TODO: revisit cleanup */
}

struct fi_provider rxr_prov = {
	.name = OFI_UTIL_PREFIX "rxr",
	.version = FI_VERSION(RXR_MAJOR_VERSION, RXR_MINOR_VERSION),
	.fi_version = RXR_FI_VERSION,
	.getinfo = rxr_getinfo,
	.fabric = rxr_fabric,
	.cleanup = rxr_fini
};

RXR_INI
{
	fi_param_define(&rxr_prov, "rx_window_size", FI_PARAM_INT,
			"Defines the maximum window size that a receiver will return for matched large messages. Defaults to the number of available posted receive buffers when the clear to send message is sent (0).");
	fi_param_define(&rxr_prov, "tx_queue_size", FI_PARAM_INT,
			"Defines the maximum number of unacknowledged sends to the core provider.");
	fi_param_define(&rxr_prov, "recvwin_size", FI_PARAM_INT,
			"Defines the size of sliding receive window.");
	fi_param_define(&rxr_prov, "cq_size", FI_PARAM_INT,
			"Define the size of completion queue.");
	fi_param_define(&rxr_prov, "inline_mr_enable", FI_PARAM_BOOL,
			"Enables inline memory registration instead of using a bounce buffer for iov's larger than max_memcpy_size. Defaults to true. When disabled, only uses a bounce buffer.");
	fi_param_define(&rxr_prov, "max_memcpy_size", FI_PARAM_INT,
			"Threshold size switch between using memory copy into a pre-registered bounce buffer and memory registration on the user buffer.");
	fi_param_define(&rxr_prov, "mtu_size", FI_PARAM_INT,
			"Override the MTU size that RxR will use with the core provider. Defaults to the core provider MTU size.");
	fi_param_define(&rxr_prov, "tx_size", FI_PARAM_INT,
			"Set the maximum number of transmit operations before the provider returns -FI_EAGAIN. If this parameter is set to a value greater than the core provider transmit context size then transmit operations will be queued when the core provider transmit queue is full.");
	fi_param_define(&rxr_prov, "rx_size", FI_PARAM_INT,
			"Set the maximum number of receive operations before the provider returns -FI_EAGAIN.");
	fi_param_define(&rxr_prov, "rx_copy_unexp", FI_PARAM_BOOL,
			"Enables the use of a separate pool of bounce-buffers to copy unexpected messages out of the buffers posted to a core's receive work queue.");
	fi_param_define(&rxr_prov, "rx_copy_ooo", FI_PARAM_BOOL,
			"Enables the use of a separate pool of bounce-buffers to copy out-of-order RTS packets out of the buffers posted to a core's receive work queue.");

	fi_param_define(&rxr_prov, "max_timeout", FI_PARAM_INT,
			"Set the maximum timeout (us) for backoff to a peer after a receiver not ready error.");
	fi_param_define(&rxr_prov, "timeout_interval", FI_PARAM_INT,
			"Set the time interval (us) for the base timeout to use for exponential backoff to a peer after a receiver not ready error.");
	rxr_init_env();
	return &rxr_prov;
}
