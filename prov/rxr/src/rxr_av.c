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

#include "rxr.h"
#include <inttypes.h>

/* insert address translation in core av & in hash */
int rxr_av_insert_rdm_addr(struct rxr_av *av, const void *addr,
			   fi_addr_t *rdm_fiaddr, uint64_t flags,
			   void *context)
{
	struct rxr_av_entry *av_entry;
	int ret;

	fastlock_acquire(&av->util_av.lock);

	HASH_FIND(hh, av->av_map, addr, av->rdm_addrlen, av_entry);

	if (!av_entry) {
		ret = fi_av_insert(av->rdm_av, addr, 1,
				   rdm_fiaddr, flags, context);
		if (OFI_UNLIKELY(ret != 1)) {
			FI_DBG(&rxr_prov, FI_LOG_AV,
			       "Error in inserting address: %s\n", fi_strerror(-ret));
			goto out;
		} else {
			av_entry = calloc(1, sizeof(*av_entry));
			if (OFI_UNLIKELY(!av_entry)) {
				ret = -FI_ENOMEM;
				FI_WARN(&rxr_prov, FI_LOG_AV,
					"Failed to allocate memory for av_entry\n");
				goto out;
			}
			memcpy(av_entry->addr, addr, av->rdm_addrlen);
			av_entry->rdm_addr = *(uint64_t *)rdm_fiaddr;
			HASH_ADD(hh, av->av_map, addr,
				 av->rdm_addrlen, av_entry);
			ret = 0;
		}
	} else {
		*rdm_fiaddr = (fi_addr_t)av_entry->rdm_addr;
		ret = 0;
	}

	FI_DBG(&rxr_prov, FI_LOG_AV,
	       "addr = %" PRIu64 " rdm_fiaddr =  %" PRIu64 "\n",
	       *(uint64_t *)addr, *rdm_fiaddr);
out:
	fastlock_release(&av->util_av.lock);
	return ret;
}

static int rxr_av_insert(struct fid_av *av_fid, const void *addr,
			 size_t count, fi_addr_t *fi_addr, uint64_t flags,
			 void *context)
{
	struct rxr_av *av;
	fi_addr_t fi_addr_res;
	int i = 0, ret = 0, success_cnt = 0;

	av = container_of(av_fid, struct rxr_av, util_av.av_fid);

	if (av->util_av.count < av->rdm_av_used + count) {
		ret = -FI_EINVAL;
		FI_WARN(&rxr_prov, FI_LOG_AV,
			"AV insert failed. Expect inserting %zu AV entries, but only %zu available\n",
			count, av->util_av.count - av->rdm_av_used);
		goto err;
	}

	for (; i < count; i++, addr = (uint8_t *)addr + av->rdm_addrlen) {
		ret = rxr_av_insert_rdm_addr(av, addr, &fi_addr_res,
					     flags, context);
		if (ret)
			break;
		if (fi_addr)
			fi_addr[i] = fi_addr_res;

		success_cnt++;
	}

	av->rdm_av_used += success_cnt;

err:
	if (OFI_UNLIKELY(ret)) {
		/* write error to event queue */
		if (av->util_av.eq)
			ofi_av_write_event(&av->util_av, i, -ret, context);
		if (fi_addr)
			fi_addr[i] = FI_ADDR_NOTAVAIL;
		i++;
	}

	/* cancel remaining request and log to event queue */
	for (; i < count ; i++) {
		if (av->util_av.eq)
			ofi_av_write_event(&av->util_av, i, FI_ECANCELED,
					   context);
		if (fi_addr)
			fi_addr[i] = FI_ADDR_NOTAVAIL;
	}

	/* update success to event queue */
	if (av->util_av.eq)
		ofi_av_write_event(&av->util_av, success_cnt, 0, context);

	return success_cnt;
}

static int rxr_av_insertsvc(struct fid_av *av, const char *node,
			    const char *service, fi_addr_t *fi_addr,
			    uint64_t flags, void *context)
{
	return -FI_ENOSYS;
}

static int rxr_av_insertsym(struct fid_av *av_fid, const char *node,
			    size_t nodecnt, const char *service, size_t svccnt,
			    fi_addr_t *fi_addr, uint64_t flags, void *context)
{
	return -FI_ENOSYS;
}

static int rxr_av_remove(struct fid_av *av_fid, fi_addr_t *fi_addr,
			 size_t count, uint64_t flags)
{
	int ret = 0;
	size_t i;
	struct rxr_av *av;
	struct rxr_av_entry *av_entry;
	void *addr;

	av = container_of(av_fid, struct rxr_av, util_av.av_fid);
	addr = calloc(1, av->rdm_addrlen);
	if (!addr) {
		FI_WARN(&rxr_prov, FI_LOG_AV,
			"Failed to allocate memory for av addr\n");
		return -FI_ENOMEM;
	}

	fastlock_acquire(&av->util_av.lock);
	for (i = 0; i < count; i++) {
		ret = fi_av_lookup(av->rdm_av, fi_addr[i],
				   addr, &av->rdm_addrlen);
		if (ret)
			break;

		ret = fi_av_remove(av->rdm_av, &fi_addr[i], 1, flags);
		if (ret)
			break;

		HASH_FIND(hh, av->av_map, addr, av->rdm_addrlen, av_entry);

		if (av_entry) {
			HASH_DEL(av->av_map, av_entry);
			free(av_entry);
		}

		av->rdm_av_used--;
	}
	fastlock_release(&av->util_av.lock);
	free(addr);
	return ret;
}

static const char *rxr_av_straddr(struct fid_av *av, const void *addr,
				  char *buf, size_t *len)
{
	struct rxr_av *rxr_av;

	rxr_av = container_of(av, struct rxr_av, util_av.av_fid);
	return rxr_av->rdm_av->ops->straddr(rxr_av->rdm_av, addr, buf, len);
}

static int rxr_av_lookup(struct fid_av *av, fi_addr_t fi_addr, void *addr,
			 size_t *addrlen)
{
	struct rxr_av *rxr_av;

	rxr_av = container_of(av, struct rxr_av, util_av.av_fid);
	return fi_av_lookup(rxr_av->rdm_av, fi_addr, addr, addrlen);
}

static struct fi_ops_av rxr_av_ops = {
	.size = sizeof(struct fi_ops_av),
	.insert = rxr_av_insert,
	.insertsvc = rxr_av_insertsvc,
	.insertsym = rxr_av_insertsym,
	.remove = rxr_av_remove,
	.lookup = rxr_av_lookup,
	.straddr = rxr_av_straddr,
};

static int rxr_av_close(struct fid *fid)
{
	struct rxr_av *av;
	struct rxr_av_entry *curr_av_entry, *tmp;
	int ret = 0;

	av = container_of(fid, struct rxr_av, util_av.av_fid);
	ret = fi_close(&av->rdm_av->fid);
	if (ret)
		goto err;

	ret = ofi_av_close(&av->util_av);
	if (ret)
		goto err;

err:
	HASH_ITER(hh, av->av_map, curr_av_entry, tmp) {
		HASH_DEL(av->av_map, curr_av_entry);
		free(curr_av_entry);
	}
	free(av);
	return ret;
}

static int rxr_av_bind(struct fid *fid, struct fid *bfid, uint64_t flags)
{
	return ofi_av_bind(fid, bfid, flags);
}

static struct fi_ops rxr_av_fi_ops = {
	.size = sizeof(struct fi_ops),
	.close = rxr_av_close,
	.bind = rxr_av_bind,
	.control = fi_no_control,
	.ops_open = fi_no_ops_open,
};

int rxr_av_open(struct fid_domain *domain_fid, struct fi_av_attr *attr,
		struct fid_av **av_fid, void *context)
{
	struct rxr_av *av;
	struct rxr_domain *domain;
	struct fi_av_attr av_attr;
	struct util_av_attr util_attr;
	int ret;

	if (!attr)
		return -FI_EINVAL;

	if (attr->name)
		return -FI_ENOSYS;

	domain = container_of(domain_fid, struct rxr_domain,
			      util_domain.domain_fid);
	av = calloc(1, sizeof(*av));
	if (!av)
		return -FI_ENOMEM;

	/*
	 * TODO: remove me once RxR supports resizing members tied to the AV
	 * size.
	 */
	if (!attr->count)
		attr->count = RXR_MIN_AV_SIZE;

	util_attr.addrlen = sizeof(fi_addr_t);
	util_attr.flags = 0;
	ret = ofi_av_init(&domain->util_domain, attr, &util_attr,
			  &av->util_av, context);
	if (ret)
		goto err;

	av_attr = *attr;

	/* Mask FI_EVENT flag as we currently do NOT support asynchronous insert */
	av_attr.flags &= ~FI_EVENT;
	FI_DBG(&rxr_prov, FI_LOG_AV, "fi_av_attr:%" PRId64 "\n",
	       av_attr.flags);

	av_attr.type = FI_AV_TABLE;

	ret = fi_av_open(domain->rdm_domain, &av_attr, &av->rdm_av, context);
	if (ret)
		goto err;

	av->rdm_addrlen = domain->addrlen;

	*av_fid = &av->util_av.av_fid;
	(*av_fid)->fid.fclass = FI_CLASS_AV;
	(*av_fid)->fid.ops = &rxr_av_fi_ops;
	(*av_fid)->ops = &rxr_av_ops;
	return 0;

err:
	free(av);
	return ret;
}
