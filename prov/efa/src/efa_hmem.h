/*
 * Copyright (c) 2022 Amazon.com, Inc. or its affiliates. All rights reserved.
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

#ifndef EFA_HMEM_H
#define EFA_HMEM_H

#include "ofi_hmem.h"
#include "efa_mr.h"

#define EFA_HMEM_IFACE_FOREACH(var) \
	for ((var) = 0; (var) < ((sizeof efa_hmem_ifaces) / (sizeof (enum fi_hmem_iface))); ++(var))

#define EFA_HMEM_IFACE_FOREACH_NON_SYSTEM(var) \
	for ((var) = 1; (var) < ((sizeof efa_hmem_ifaces) / (sizeof (enum fi_hmem_iface))); ++(var))

/* Order matters */
static const enum fi_hmem_iface efa_hmem_ifaces[] = {
	FI_HMEM_SYSTEM,	/* Must be first here */
	FI_HMEM_CUDA,
	FI_HMEM_NEURON,
	FI_HMEM_SYNAPSEAI
};

struct efa_hmem_info {
	bool initialized; 	/* do we support it at all */
	bool p2p_disabled_by_user;	/* Did the user disable p2p via FI_OPT_FI_HMEM_P2P? */
	bool p2p_required_by_impl;	/* Is p2p required for this interface? */
	bool p2p_supported_by_device;	/* do we support p2p with this device */

	size_t max_intra_eager_size; /* Maximum message size to use eager protocol for intra-node */
	size_t max_medium_msg_size;
	size_t runt_size;
	size_t min_read_msg_size;
	size_t min_read_write_size;

	/**
	 * @brief number of messages that are using read based protocol. used as a semaphore for creating runts. do we need to synchronize access to this? use an actual semaphore?
	 * need to init this somewhere!!!!!
	 */
	int64_t num_read_msg_in_flight;

};

struct efa_domain;

int efa_domain_hmem_validate_p2p_opt(struct efa_domain *efa_domain, enum fi_hmem_iface iface, int p2p_opt);
int efa_domain_hmem_info_init_all(struct efa_domain *efa_domain);

/**
 * @brief Copy data from a hmem device to a system buffer
 *
 * @param[in]    desc            Pointer to a memory registration descriptor
 * @param[out]   buff            Destination system memory buffer
 * @param[in]    src             Source hmem device memory
 * @param[in]    size            Data size in bytes to copy
 * @return       FI_SUCCESS status code on success, or an error code.
 */
static inline int efa_copy_from_hmem(void *desc, void *buff, const void *src, size_t size)
{
	uint64_t device = 0, flags = 0;
	enum fi_hmem_iface iface = FI_HMEM_SYSTEM;
	void *hmem_data = NULL;

	if (desc) {
		iface = ((struct efa_mr *)desc)->peer.iface;
		device = ((struct efa_mr *)desc)->peer.device.reserved;
		flags = ((struct efa_mr *)desc)->peer.flags;
		hmem_data = ((struct efa_mr *)desc)->peer.hmem_data;
	}

	if (FI_HMEM_CUDA == iface && (flags & OFI_HMEM_DATA_DEV_REG_HANDLE)) {
		assert(hmem_data);
		/* TODO: Fine tune the max data size to switch from gdrcopy to cudaMemcpy */
		cuda_gdrcopy_from_dev((uint64_t)hmem_data, buff, src, size);
		return FI_SUCCESS;
	}

	return ofi_copy_from_hmem(iface, device, buff, src, size);
};

/**
 * @brief Copy data from a system buffer to a hmem device
 *
 * @param[in]    desc            Pointer to a memory registration descriptor
 * @param[out]   dest            Destination hmem device memory
 * @param[in]    buff            Source system memory buffer
 * @param[in]    size            Data size in bytes to copy
 * @return       FI_SUCCESS status code on success, or an error code.
 */
static inline int efa_copy_to_hmem(void *desc, void *dest, const void *buff, size_t size)
{
	uint64_t device = 0, flags = 0;
	enum fi_hmem_iface iface = FI_HMEM_SYSTEM;
	void *hmem_data = NULL;

	if (desc) {
		iface = ((struct efa_mr *)desc)->peer.iface;
		device = ((struct efa_mr *)desc)->peer.device.reserved;
		flags = ((struct efa_mr *)desc)->peer.flags;
		hmem_data = ((struct efa_mr *)desc)->peer.hmem_data;
	}

	if (FI_HMEM_CUDA == iface && (flags & OFI_HMEM_DATA_DEV_REG_HANDLE)) {
		assert(hmem_data);
		/* TODO: Fine tune the max data size to switch from gdrcopy to cudaMemcpy */
		cuda_gdrcopy_to_dev((uint64_t)hmem_data, dest, buff, size);
		return FI_SUCCESS;
	}

	return ofi_copy_to_hmem(iface, device, dest, buff, size);
};

ssize_t efa_copy_from_hmem_iov(void **desc, char *buff, int buff_size, const struct iovec *hmem_iov, int iov_count);
ssize_t efa_copy_to_hmem_iov(void **desc, struct iovec *hmem_iov, int iov_count, char *buff, int buff_size);
#endif
