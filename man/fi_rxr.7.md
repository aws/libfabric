---
layout: page
title: fi_rxr(7)
tagline: Libfabric Programmer's Manual
---
{% include JB/setup %}

# NAME

fi_rxr \- The RxR (RDM over RDM) Utility Provider

# OVERVIEW

This is a utility provider that extends the capabilities of core
providers that support reliable datagram endpoints. In addition to
supporting the capabilities that a core provider offers, RxR extends
them to support tagged data transfer operations, segmentation of
messages larger than a core's maximum supported message size, and
reassembly of out-of-order packets to provide send-after-send ordering
guarantees to applications.

# SUPPORTED FEATURES

The RxR provider currently supports *FI_MSG* and *FI_TAGGED* data
transfer capabilities. Additionally, RxR will provide a maximum message
size of 2^64 - 1 bytes. RxR requires the base RDM provider to support
*FI_MSG* capabilities.

*Endpoint types*
: The provider supports only endpoint type *FI_EP_RDM*.

*Endpoint capabilities*
: The following data transfer interfaces are supported: *FI_MSG* and
  *FI_TAGGED*.  *FI_SEND*, *FI_RECV*, *FI_DIRECTED_RECV*, *FI_MULTI_RECV*,
  and *FI_SOURCE* capabilities are supported. The reliable endpoint provides
  send-after-send guarantees for data operations.

*Address vectors*
: The provider supports both the *FI_AV_MAP* and *FI_AV_TABLE* address
  vector types.  *FI_EVENT* is unsupported.

*Completion events*
: The provider supports *FI_CQ_FORMAT_CONTEXT*, *FI_CQ_FORMAT_MSG*,
  *FI_CQ_FORMAT_DATA* and *FI_CQ_FORMAT_TAGGED*.  Wait objects are not
  currently supported.

*Modes*
: The provider does not require the use of any mode bits.

*Memory registration modes*
: The RxR provider supports *FI_MR_SCALABLE*, even if the base
  provider only supports *FI_MR_LOCAL*.

*Progress*
: The RxR provider supports both *FI_PROGRESS_AUTO* and
  *FI_PROGRESS_MANUAL*, with a default set to *FI_PROGRESS_MANUAL*.
  However, receive side data buffers are not modified outside of
  completion processing routines.

# LIMITATIONS

The RxR provider has hard-coded maximums for supported queue sizes and
data transfers. Some of these limits are derived at runtime from the
base RDM provider that RxR layers over.

The provider does not support `FI_RMA` and `FI_ATOMIC` capabilities..

# RUNTIME PARAMETERS

*rx_window_size*
: Maximum number of MTU-sized messages that can be in flight from any
  single endpoint as part of long message data transfer.

*tx_queue_size*
: Depth of RxR transmit queue.  RxR will queue (and progress according
  to the active progress model) transmit *tx_queue_size* requests
  beyond any queueing in the base provider.

*recvwin_size*
: Size of out of order reorder buffer (in messages).  Messages
  received out of this window will result in an error.

*cq_size*
: Size of any cq created, in number of entries.

*inline_mr_enable*
: Enables inline memory registration instead of using a bounce buffer
  for iov's larger than max_memcpy_size. Defaults to true. When disabled,
  only uses a bounce buffer.

*max_memcpy_size*
: Threshold size switch between using memory copy into a pre-registered
  bounce buffer and memory registration on the user buffer.

*mtu_size*
: MTU size that RxR will use with the core provider.
  Defaults to the core provider MTU size.

*tx_size*
: Maximum number of transmit operations before the provider
  returns -FI_EAGAIN. If this parameter is set to a value greater than
  the core provider transmit context size then transmit operations will be
  queued when the core provider transmit queue is full.

*rx_size*
: Maximum number of receive operations before the provider
  returns -FI_EAGAIN.

*rx_copy_unexp*
: Enables the use of a separate pool of bounce-buffers to copy
  unexpected messages out of the buffers posted to a core's receive work
  queue.

*rx_copy_ooo*
: Enables the use of a separate pool of bounce-buffers to copy
  out-of-order RTS packets out of the buffers posted to a core's receive
  work queue.

*max_timeout*
: Maximum timeout (us) for backoff to a peer after a receiver not ready error.

*timeout_interval*
: Time interval (us) for the base timeout to use for exponential backoff
  to a peer after a receiver not ready error.

# SEE ALSO

[`fabric`(7)](fabric.7.html),
[`fi_provider`(7)](fi_provider.7.html),
[`fi_getinfo`(3)](fi_getinfo.3.html)
