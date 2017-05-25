/*
 * Copyright (c) 2017-2019 Amazon.com, Inc. or its affiliates. All rights reserved.
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

#include "config.h"
#include <ofi_util.h>
#include <ofi_uffd.h>
#include "efa.h"
#include "efa_verbs.h"

#if HAVE_USERFAULTFD_UNMAP

static void efa_mem_notifier_handle_uffd(void *arg, RbtIterator iter)
{
	struct iovec *key;
	struct efa_subscr_entry *subscr_entry;
	struct efa_monitor_entry *entry;

	rbtKeyValue(efa_mem_notifier->subscr_storage, iter, (void *)&key,
		    (void *)&entry);
	dlist_foreach_container(&entry->subscription_list,
				struct efa_subscr_entry, subscr_entry, entry)
		ofi_monitor_add_event_to_nq(subscr_entry->subscription);

	EFA_DBG(FI_LOG_MR, "Adding events to notification queues for region %p:%lu\n",
		key->iov_base, key->iov_len);
}

static inline void efa_uffd_add_events(struct efa_mem_notifier *notifier,
				       struct iovec *iov)
{
	RbtIterator iter;

	iter = rbtFind(notifier->subscr_storage, (void *)iov);
	if (iter) {
		EFA_DBG(FI_LOG_MR, "Userfault for memory %p:%lu\n",
			iov->iov_base, iov->iov_len);
		rbtTraversal(efa_mem_notifier->subscr_storage, iter, NULL,
			     efa_mem_notifier_handle_uffd);
	}
}

static inline void efa_notifier_process_events(struct efa_mem_notifier *notifier)
{
	struct efa_mem_region *efa_mem_region;

	dlist_foreach_container(&notifier->fault_list, struct efa_mem_region,
				efa_mem_region, entry) {
		efa_uffd_add_events(notifier, &efa_mem_region->iov);
		dlist_remove(&efa_mem_region->entry);
		util_buf_release(notifier->fault_pool, efa_mem_region);
	}
}

/* Clear the events without adding to notification queues */
static inline void efa_notifier_flush_events(struct efa_mem_notifier *notifier)
{
	struct efa_mem_region *efa_mem_region;

	dlist_foreach_container(&notifier->fault_list, struct efa_mem_region,
				efa_mem_region, entry) {
		dlist_remove(&efa_mem_region->entry);
		util_buf_release(notifier->fault_pool, efa_mem_region);
	}
}

static int efa_mr_cache_close(fid_t fid)
{
	struct efa_mem_desc *mr = container_of(fid, struct efa_mem_desc,
					       mr_fid.fid);

	fastlock_acquire(&efa_mem_notifier->lock);

	efa_notifier_process_events(efa_mem_notifier);
	ofi_mr_cache_delete(&mr->domain->cache, mr->entry);

	fastlock_release(&efa_mem_notifier->lock);
	return 0;
}

int efa_monitor_subscribe(struct ofi_mem_monitor *monitor,
			  struct ofi_subscription *subscription)
{
	struct efa_domain *domain = container_of(monitor, struct efa_domain,
						 monitor);
	struct efa_subscr_entry *subscr_entry;
	struct efa_monitor_entry *entry;
	int ret = FI_SUCCESS;
	struct iovec *key;
	RbtStatus rbt_ret;
	RbtIterator iter;

	entry = calloc(1, sizeof(*entry));
	if (OFI_UNLIKELY(!entry))
		return -FI_ENOMEM;

	entry->iov = subscription->iov;
	dlist_init(&entry->subscription_list);

	rbt_ret = rbtInsert(domain->notifier->subscr_storage,
			    (void *)&entry->iov, (void *)entry);
	switch (rbt_ret) {
	case RBT_STATUS_DUPLICATE_KEY:
		free(entry);
		iter = rbtFind(domain->notifier->subscr_storage,
			       (void *)&subscription->iov);
		assert(iter);
		rbtKeyValue(domain->notifier->subscr_storage, iter,
			    (void *)&key, (void *)&entry);
		/* fall through */
	case RBT_STATUS_OK:
		if (rbt_ret == RBT_STATUS_OK) {
			ret = ofi_uffd_register(domain->notifier->uffd,
						(uint64_t)subscription->iov.iov_base,
						subscription->iov.iov_len);
			if (ret)
				break;
		}
		subscr_entry = calloc(1, sizeof(*subscr_entry));
		if (OFI_LIKELY(subscr_entry != NULL)) {
			subscr_entry->subscription = subscription;
			dlist_insert_tail(&subscr_entry->entry,
					  &entry->subscription_list);
			break;
		}
		/* Do not free monitor entry in case of duplicate key */
		if (rbt_ret == RBT_STATUS_OK) {
			iter = rbtFind(domain->notifier->subscr_storage,
				       (void *)&subscription->iov);
			assert(iter);
			rbtErase(domain->notifier->subscr_storage, iter);
			free(entry);
		}
		/* fall through */
	default:
		ret = -FI_EAVAIL;
		break;
	}

	return ret;
}

void efa_monitor_unsubscribe(struct ofi_mem_monitor *monitor,
			     struct ofi_subscription *subscription)
{
	struct efa_domain *domain = container_of(monitor, struct efa_domain,
						 monitor);
	RbtIterator iter;
	struct efa_subscr_entry *subscr_entry;
	struct efa_monitor_entry *entry;
	struct iovec *key;

	iter = rbtFind(domain->notifier->subscr_storage,
		       (void *)&subscription->iov);
	assert(iter);
	rbtKeyValue(domain->notifier->subscr_storage, iter,
		    (void *)&key, (void *)&entry);
	dlist_foreach_container(&entry->subscription_list,
				struct efa_subscr_entry,
				subscr_entry, entry) {
		if (subscr_entry->subscription == subscription) {
			dlist_remove(&subscr_entry->entry);
			free(subscr_entry);
			break;
		}
	}

	if (dlist_empty(&entry->subscription_list)) {
		rbtErase(domain->notifier->subscr_storage, iter);
		free(entry);
		/* May already be unregistered. Ignore return value */
		ofi_uffd_unregister(domain->notifier->uffd,
				    (uint64_t)subscription->iov.iov_base,
				    subscription->iov.iov_len);
	}
}

static void efa_add_fault_to_list(struct efa_mem_notifier *notifier,
				  uintptr_t start, size_t len)
{
	struct efa_mem_region *efa_mem_region;

	EFA_DBG(FI_LOG_MR, "Add fault to list start=%p end=%zu\n",
		(void *)start, len);

	efa_mem_region = util_buf_alloc(notifier->fault_pool);
	if (!efa_mem_region) {
		EFA_WARN(FI_LOG_MR,
			 "Unable to allocate memory for fault entry, aborting...\n");
		abort();
	}

	efa_mem_region->iov.iov_base = (void *)start;
	efa_mem_region->iov.iov_len = len;

	dlist_insert_tail(&efa_mem_region->entry, &notifier->fault_list);

	ofi_uffd_unregister(notifier->uffd, start, len);
}

static void *efa_uffd_handler(void *arg)
{
	struct efa_mem_notifier *notifier = arg;
	struct uffd_msg msg;
	fd_set uffd_set;
	int ret;

	while (1) {
		FD_ZERO(&uffd_set);
		FD_SET(notifier->uffd, &uffd_set);
		ret = select(notifier->uffd + 1, &uffd_set, NULL, NULL, NULL);
		if (OFI_UNLIKELY(ret == -1)) {
			if (errno == EAGAIN)
				continue;
			EFA_WARN(FI_LOG_MR,
				 "select/userfaultfd: %s\n", strerror(errno));
			abort();
		}

		fastlock_acquire(&notifier->lock);

		ret = read(notifier->uffd, &msg, sizeof(msg));
		if (ret == -1) {
			if (errno == EAGAIN) {
				fastlock_release(&notifier->lock);
				continue;
			}
			EFA_WARN(FI_LOG_MR,
				 "read/userfaultfd: %s.\n", strerror(errno));
			abort();
		}

		if (ret != sizeof(msg)) {
			EFA_WARN(FI_LOG_MR,
				 "read/userfaultfd: Invalid message size. Aborting.\n");
			abort();
		}

		switch (msg.event) {
		case UFFD_EVENT_REMOVE:
		case UFFD_EVENT_UNMAP:
			efa_add_fault_to_list(notifier,
					      msg.arg.remove.start,
					      msg.arg.remove.end -
					      msg.arg.remove.start);
			break;
		case UFFD_EVENT_REMAP:
			efa_add_fault_to_list(notifier,
					      msg.arg.remap.from,
					      msg.arg.remap.len);
			break;
		case UFFD_EVENT_PAGEFAULT:
			EFA_WARN(FI_LOG_MR,
				 "Page fault occurred on a registered region.\n");
			abort();
			break;
		default:
			EFA_WARN(FI_LOG_MR,
				 "Unable to handle userfault fd event. Aborting.\n");
			abort();
			break;
		}

		fastlock_release(&notifier->lock);
	}
}

static int efa_uffd_init(struct efa_mem_notifier *notifier)
{
	int flags = O_CLOEXEC | O_NONBLOCK;
	/* HUGETLBFS support will automatically detected */
	int features = UFFD_FEATURE_EVENT_UNMAP |
		       UFFD_FEATURE_EVENT_REMOVE |
		       UFFD_FEATURE_EVENT_REMAP;
	int ret;

	notifier->uffd = ofi_uffd_init(flags, features);

	if (notifier->uffd < 0)
		return notifier->uffd;

	ret = pthread_create(&notifier->uffd_thread, NULL, efa_uffd_handler,
			     notifier);
	if (ret)
		ofi_uffd_close(notifier->uffd);

	return ret;
}

static void efa_uffd_cleanup(struct efa_mem_notifier *notifier)
{
	pthread_cancel(notifier->uffd_thread);
	pthread_join(notifier->uffd_thread, NULL);
	ofi_uffd_close(notifier->uffd);
	notifier->uffd = -1;
}

int efa_mem_notifier_init(void)
{
	int ret = 0;

	efa_mem_notifier = calloc(1, sizeof(*efa_mem_notifier));

	if (!efa_mem_notifier) {
		EFA_DBG(FI_LOG_MR,
			"No memory error: efa_mem_notifier is NULL.\n");
		return -FI_ENOMEM;
	}

	efa_mem_notifier->subscr_storage =
		rbtNew(efa_mr_cache_merge_regions ?
		       ofi_mr_find_overlap :
		       ofi_mr_find_within);

	if (!efa_mem_notifier->subscr_storage) {
		EFA_DBG(FI_LOG_MR,
			"Couldn't create rbt subscription storage.\n");
		ret = -FI_ENOMEM;
		goto err_free_notifier;
	}

	ret = util_buf_pool_create(&efa_mem_notifier->fault_pool,
				   sizeof(struct efa_mem_region),
				   EFA_MEM_ALIGNMENT,
				   0,
				   EFA_FAULT_CNT);
	if (ret)
		goto err_delete_storage;

	dlist_init(&efa_mem_notifier->fault_list);

	if (fastlock_init(&efa_mem_notifier->lock)) {
		EFA_DBG(FI_LOG_MR, "Couldn't initialize mem notifier lock.\n");
		ret = -FI_ENOMEM;
		goto err_destroy_util_buf;
	}

	ret = efa_uffd_init(efa_mem_notifier);
	if (ret) {
		EFA_DBG(FI_LOG_MR,
			"Couldn't initialize userfault_fd thread.\n");
		goto err_destroy_lock;
	}
	return ret;
err_destroy_lock:
	fastlock_destroy(&efa_mem_notifier->lock);
err_destroy_util_buf:
	util_buf_pool_destroy(efa_mem_notifier->fault_pool);
err_delete_storage:
	rbtDelete(efa_mem_notifier->subscr_storage);
err_free_notifier:
	free(efa_mem_notifier);
	efa_mem_notifier = NULL;
	return ret;
}

void efa_mem_notifier_finalize(void)
{
	if (efa_mem_notifier) {
		fastlock_acquire(&efa_mem_notifier->lock);
		/* Flush the uffd event list */
		efa_notifier_flush_events(efa_mem_notifier);
		/* No notifier calls remaining in use */
		rbtDelete(efa_mem_notifier->subscr_storage);
		efa_uffd_cleanup(efa_mem_notifier);
		fastlock_release(&efa_mem_notifier->lock);
		fastlock_destroy(&efa_mem_notifier->lock);
		util_buf_pool_destroy(efa_mem_notifier->fault_pool);
		free(efa_mem_notifier);
		efa_mem_notifier = NULL;
	}
}

static int efa_mr_cache_reg(struct fid *fid, const void *buf, size_t len,
			    uint64_t access, uint64_t offset,
			    uint64_t requested_key, uint64_t flags,
			    struct fid_mr **mr_fid, void *context)
{
	struct efa_domain *domain;
	struct efa_mem_desc *md;
	struct ofi_mr_entry *entry;
	int ret;

	struct iovec iov = {
		.iov_base	= (void *)buf,
		.iov_len	= len,
	};

	struct fi_mr_attr attr = {
		.mr_iov		= &iov,
		.iov_count	= 1,
		.access		= access,
		.offset		= offset,
		.requested_key	= requested_key,
		.context	= context,
	};

	if (access & ~EFA_MR_SUPPORTED_PERMISSIONS) {
		EFA_WARN(FI_LOG_MR,
			 "Unsupported access permissions. requested[0x%" PRIx64 "] supported[0x%" PRIx64 "]\n",
			 access, (uint64_t)EFA_MR_SUPPORTED_PERMISSIONS);
		return -FI_EINVAL;
	}

	domain = container_of(fid, struct efa_domain,
			      util_domain.domain_fid.fid);

	fastlock_acquire(&efa_mem_notifier->lock);

	efa_notifier_process_events(efa_mem_notifier);
	ret = ofi_mr_cache_search(&domain->cache, &attr, &entry);

	fastlock_release(&efa_mem_notifier->lock);

	if (OFI_UNLIKELY(ret))
		return ret;

	md = (struct efa_mem_desc *)entry->data;
	md->entry = entry;

	*mr_fid = &md->mr_fid;
	return 0;
}
#else /* !HAVE_USERFAULTFD_UNMAP */

static int efa_mr_cache_close(fid_t fid)
{
	OFI_UNUSED(fid);
	return -FI_ENOSYS;
}

int efa_monitor_subscribe(struct ofi_mem_monitor *monitor,
			  struct ofi_subscription *subscription)
{
	OFI_UNUSED(monitor);
	OFI_UNUSED(subscription);
	return -FI_ENOSYS;
}

void efa_monitor_unsubscribe(struct ofi_mem_monitor *monitor,
			     struct ofi_subscription *subscription)
{
	OFI_UNUSED(monitor);
	OFI_UNUSED(subscription);
}

int efa_mem_notifier_init(void)
{
	return -FI_ENOSYS;
}

void efa_mem_notifier_finalize(void)
{
	/* Nothing to cleanup */
}

static int efa_mr_cache_reg(struct fid *fid, const void *buf, size_t len,
			    uint64_t access, uint64_t offset,
			    uint64_t requested_key, uint64_t flags,
			    struct fid_mr **mr_fid, void *context)
{
	OFI_UNUSED(fid);
	OFI_UNUSED(buf);
	OFI_UNUSED(len);
	OFI_UNUSED(access);
	OFI_UNUSED(offset);
	OFI_UNUSED(requested_key);
	OFI_UNUSED(flags);
	OFI_UNUSED(mr_fid);
	OFI_UNUSED(context);
	return -FI_ENOSYS;
}
#endif /* HAVE_USERFAULTFD_UNMAP */

static int efa_mr_close(fid_t fid)
{
	struct efa_mem_desc *mr;
	int ret;

	mr = container_of(fid, struct efa_mem_desc, mr_fid.fid);
	ret = -efa_cmd_dereg_mr(mr->mr);
	if (!ret)
		free(mr);
	return ret;
}

static struct fi_ops efa_mr_ops = {
	.size = sizeof(struct fi_ops),
	.close = efa_mr_close,
	.bind = fi_no_bind,
	.control = fi_no_control,
	.ops_open = fi_no_ops_open,
};

static struct fi_ops efa_mr_cache_ops = {
	.size = sizeof(struct fi_ops),
	.close = efa_mr_cache_close,
	.bind = fi_no_bind,
	.control = fi_no_control,
	.ops_open = fi_no_ops_open,
};

static int efa_mr_reg(struct fid *fid, const void *buf, size_t len,
		      uint64_t access, uint64_t offset, uint64_t requested_key,
		      uint64_t flags, struct fid_mr **mr_fid, void *context)
{
	struct fid_domain *domain_fid;
	struct efa_mem_desc *md;
	int fi_ibv_access = 0;

	if (flags)
		return -FI_EBADFLAGS;

	if (fid->fclass != FI_CLASS_DOMAIN)
		return -FI_EINVAL;

	if (access & ~EFA_MR_SUPPORTED_PERMISSIONS) {
		EFA_WARN(FI_LOG_MR,
			 "Unsupported access permissions. requested[0x%" PRIx64 "] supported[0x%" PRIx64 "]\n",
			 access, (uint64_t)EFA_MR_SUPPORTED_PERMISSIONS);
		return -FI_EINVAL;
	}

	domain_fid = container_of(fid, struct fid_domain, fid);

	md = calloc(1, sizeof(*md));
	if (!md)
		return -FI_ENOMEM;

	md->domain = container_of(domain_fid, struct efa_domain,
				  util_domain.domain_fid);
	md->mr_fid.fid.fclass = FI_CLASS_MR;
	md->mr_fid.fid.context = context;
	md->mr_fid.fid.ops = &efa_mr_ops;

	/* Local read access to an MR is enabled by default in verbs */
	if (access & FI_RECV)
		fi_ibv_access |= IBV_ACCESS_LOCAL_WRITE;

	md->mr = efa_cmd_reg_mr(md->domain->pd, (void *)buf, len, fi_ibv_access);
	if (!md->mr) {
		EFA_WARN_ERRNO(FI_LOG_MR, "efa_cmd_reg_mr", errno);
		goto err;
	}

	md->mr_fid.mem_desc = (void *)(uintptr_t)md->mr->lkey;
	md->mr_fid.key = md->mr->rkey;
	*mr_fid = &md->mr_fid;

	return 0;

err:
	free(md);
	return -errno;
}


static int efa_mr_regv(struct fid *fid, const struct iovec *iov,
		       size_t count, uint64_t access, uint64_t offset, uint64_t requested_key,
		       uint64_t flags, struct fid_mr **mr_fid, void *context)
{
	if (count > EFA_MR_IOV_LIMIT) {
		EFA_WARN(FI_LOG_MR, "iov count > %d not supported\n",
			 EFA_MR_IOV_LIMIT);
		return -FI_EINVAL;
	}
	return efa_mr_reg(fid, iov->iov_base, iov->iov_len, access, offset,
			  requested_key, flags, mr_fid, context);
}

static int efa_mr_cache_regv(struct fid *fid, const struct iovec *iov,
			     size_t count, uint64_t access, uint64_t offset,
			     uint64_t requested_key, uint64_t flags,
			     struct fid_mr **mr_fid, void *context)
{
	if (count > EFA_MR_IOV_LIMIT) {
		EFA_WARN(FI_LOG_MR, "iov count > %d not supported\n",
			 EFA_MR_IOV_LIMIT);
		return -FI_EINVAL;
	}
	return efa_mr_cache_reg(fid, iov->iov_base, iov->iov_len, access,
				offset, requested_key, flags, mr_fid, context);
}

static int efa_mr_regattr(struct fid *fid, const struct fi_mr_attr *attr,
			  uint64_t flags, struct fid_mr **mr_fid)
{
	return efa_mr_regv(fid, attr->mr_iov, attr->iov_count, attr->access,
			   attr->offset, attr->requested_key, flags, mr_fid,
			   attr->context);
}

static int efa_mr_cache_regattr(struct fid *fid, const struct fi_mr_attr *attr,
				uint64_t flags, struct fid_mr **mr_fid)
{
	return efa_mr_cache_regv(fid, attr->mr_iov, attr->iov_count,
				 attr->access, attr->offset,
				 attr->requested_key, flags, mr_fid,
				 attr->context);
}

int efa_mr_cache_entry_reg(struct ofi_mr_cache *cache,
			   struct ofi_mr_entry *entry)
{
	int fi_ibv_access = IBV_ACCESS_LOCAL_WRITE;

	struct efa_mem_desc *md = (struct efa_mem_desc *)entry->data;

	md->domain = container_of(cache->domain, struct efa_domain,
				  util_domain);
	md->mr_fid.fid.ops = &efa_mr_cache_ops;

	md->mr_fid.fid.fclass = FI_CLASS_MR;
	md->mr_fid.fid.context = NULL;

	md->mr = efa_cmd_reg_mr(md->domain->pd, entry->iov.iov_base,
				entry->iov.iov_len, fi_ibv_access);
	if (!md->mr) {
		EFA_WARN_ERRNO(FI_LOG_MR, "efa_cmd_reg_mr", errno);
		return -errno;
	}

	md->mr_fid.mem_desc = (void *)(uintptr_t)md->mr->lkey;
	md->mr_fid.key = md->mr->rkey;

	return 0;
}

void efa_mr_cache_entry_dereg(struct ofi_mr_cache *cache,
			      struct ofi_mr_entry *entry)
{
	struct efa_mem_desc *md = (struct efa_mem_desc *)entry->data;
	int ret = -efa_cmd_dereg_mr(md->mr);
	if (ret)
		EFA_WARN(FI_LOG_MR, "Unable to dereg mr: %d\n", ret);
}

struct fi_ops_mr efa_domain_mr_ops = {
	.size = sizeof(struct fi_ops_mr),
	.reg = efa_mr_reg,
	.regv = efa_mr_regv,
	.regattr = efa_mr_regattr,
};

struct fi_ops_mr efa_domain_mr_cache_ops = {
	.size = sizeof(struct fi_ops_mr),
	.reg = efa_mr_cache_reg,
	.regv = efa_mr_cache_regv,
	.regattr = efa_mr_cache_regattr,
};
