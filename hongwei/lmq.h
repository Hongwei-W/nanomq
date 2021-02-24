//
// Copyright 2019 Staysail Systems, Inc. <info@staysail.tech>
//
// This software is supplied under the terms of the MIT License, a
// copy of which should be located in the distribution where this
// file was obtained (LICENSE.txt).  A copy of the license may also be
// found online at https://opensource.org/licenses/MIT.
//

#ifndef LMQ_H
#define LMQ_H

#define NNG_ENOMEM -3
#define NNG_ENOENT -4
#define NNG_EAGAIN -5

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "message.h"
// nni_lmq is a very lightweight message queue.  Defining it this way allows
// us to share some common code.  Locking must be supplied by the caller.
// For performance reasons, this is allocated inline.

struct nng_msg {
	//uint8_t m_header_buf[NNI_NANO_MAX_HEADER_SIZE + 1]; // only fixed header
	// uint8_t       m_variable_header_buf[];                //TODO
	// independent variable header?
	//size_t         m_header_len;
	//nni_chunk      m_body;
	//uint32_t       m_pipe; // set on receive
	//nni_atomic_int m_refcnt;
	// FOR NANOMQ
	//size_t  remaining_len;
	//uint8_t CMD_TYPE;
	// uint8_t          *variable_ptr;         //equal to m_body
	//uint8_t *        payload_ptr; // payload
	//nni_time         times;
	nano_conn_param *cparam;
	//uint8_t          qos;
};

typedef struct nng_msg  nng_msg;
typedef nng_msg           nni_msg;

typedef struct nni_lmq {
	size_t    lmq_cap;
	size_t    lmq_alloc; // alloc is cap, rounded up to power of 2
	size_t    lmq_mask;
	size_t    lmq_len;
	size_t    lmq_get;
	size_t    lmq_put;
	nng_msg **lmq_msgs;
} nni_lmq;

/*
typedef struct {
	size_t   ch_cap; // allocated size
	size_t   ch_len; // length in use
	uint8_t *ch_buf; // underlying buffer
	uint8_t *ch_ptr; // pointer to actual data
} nni_chunk;
*/



extern int    nni_lmq_init(nni_lmq *, size_t);
extern void   nni_lmq_fini(nni_lmq *);
extern void   nni_lmq_flush(nni_lmq *);
extern size_t nni_lmq_len(nni_lmq *);
extern size_t nni_lmq_cap(nni_lmq *);
extern int    nni_lmq_putq(nni_lmq *, nng_msg *);
extern int    nni_lmq_getq(nni_lmq *, nng_msg **);
extern int    nni_lmq_resize(nni_lmq *, size_t);
extern bool   nni_lmq_full(nni_lmq *);
extern bool   nni_lmq_empty(nni_lmq *);

#endif // CORE_LMQ_H
