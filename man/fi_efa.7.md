---
layout: page
title: fi_efa(7)
tagline: Libfabric Programmer's Manual
---
{% include JB/setup %}

# NAME

fi_efa \- The Amazon Elastic Fabric Adapter (EFA) Provider

# OVERVIEW

The EFA provider supports the Elastic Fabric Adapter (EFA) device on
Amazon EC2.  EFA provides reliable and unreliable datagram send/receive
with direct hardware access from userspace (OS bypass).

# SUPPORTED FEATURES

The following features are supported:

*Endpoint types*
: The provider supports endpoint type *FI_EP_DGRAM*, and *FI_EP_RDM* on a new
  Scalable (unordered) Reliable Datagram protocol (SRD). SRD provides support
  for reliable datagrams and more complete error handling than typically seen
  with other Reliable Datagram (RD) implementations, but, unlike RD, it does not
  support ordering or segmentation.
  Ordering can be achieved by layering RxR over EFA.

*Endpoint capabilities*
: The provider only supports *FI_MSG* capability with a maximum message size of
  the MTU of the underlying hardware (approximately 8 KiB).

*Address vectors*
: The provider supports *FI_AV_TABLE* and *FI_AV_MAP* address vector types.
  *FI_EVENT* is unsupported.

*Completion events*
: The provider supports *FI_CQ_FORMAT_CONTEXT*, *FI_CQ_FORMAT_MSG*, and
  *FI_CQ_FORMAT_DATA*.  Wait objects are not
  currently supported.

*Modes*
: The provider requires the use of *FI_MSG_PREFIX* when running over
  *FI_EP_DGRAM* endpoint, and requires *FI_MR_LOCAL* for all memory
  registrations.

*Memory registration modes*
: The provider only supports *FI_MR_LOCAL*.

*Progress*
: The provider only supports *FI_PROGRESS_MANUAL*.

*Threading*
: The supported mode is *FI_THREAD_DOMAIN*, i.e. the provider is not
  thread safe.

# LIMITATIONS

The provider does not support *FI_TAGGED*, *FI_RMA*, or *FI_ATOMIC*
interfaces. The provider does not fully protect against resource
overruns, so resource management is disabled (*FI_RM_DISABLED*).

No support for selective completions or multi-recv.

No support for counters.

No support for inject.

# RUNTIME PARAMETERS

*FI_EFA_TX_SIZE*
: Default maximum tx context size.

*FI_EFA_RX_SIZE*
: Default maximum rx context size.

*FI_EFA_TX_IOV_LIMIT*
: Default maximum tx iov_limit.

*FI_EFA_RX_IOV_LIMIT*
: Default maximum rx iov_limit

*FI_EFA_INLINE_SIZE*
: Default maximum inline size.

*FI_EFA_MR_CACHE_ENABLE*
: Enable Memory Region caching.

*FI_EFA_MR_MAX_CACHED_COUNT*
: Maximum number of cache entries.

*FI_EFA_MR_MAX_CACHED_SIZE*
: Maximum total size of cache entries.

*FI_EFA_MR_CACHE_MERGE_REGIONS*
: Enable the merging of MR regions for MR caching functionality.

# SEE ALSO

[`fabric`(7)](fabric.7.html),
[`fi_provider`(7)](fi_provider.7.html),
[`fi_getinfo`(3)](fi_getinfo.3.html)
