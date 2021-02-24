//
// Copyright 2020 Staysail Systems, Inc. <info@staysail.tech>
//
// This software is supplied under the terms of the MIT License, a
// copy of which should be located in the distribution where this
// file was obtained (LICENSE.txt).  A copy of the license may also be
// found online at https://opensource.org/licenses/MIT.
//

//#include "nng_impl.h"

#include "lmq.h"

//#include "message.c"


// Light-weight message queue. These are derived from our heavy-weight
// message queues, but are less "featureful", but more useful for
// performance sensitive contexts.  Locking must be done by the caller.

// from defs.h starts

#define NNI_ARG_UNUSED(x) ((void) x)
void *
nni_alloc(size_t sz)
{
	return (sz > 0 ? malloc(sz) : NULL);
}

/*
void *
nni_zalloc(size_t sz)
{
	return (sz > 0 ? calloc(1, sz) : NULL);
}

void
nni_free(void *b, size_t z)
{
	NNI_ARG_UNUSED(z);
	free(b);
}
*/

#define NNI_FREE_STRUCT(s) nni_free((s), sizeof(*s))
#define NNI_FREE_STRUCTS(s, n) nni_free(s, sizeof(*s) * n)
#define NNI_ALLOC_STRUCT(s) nni_zalloc(sizeof(*s))
#define NNI_ALLOC_STRUCTS(s, n) nni_zalloc(sizeof(*s) * n)

// from defs.h ends


// from message.c starts 


void
nni_msg_free(nni_msg *m)
{
	//if ((m != NULL) && (nni_atomic_dec_nv(&m->m_refcnt) == 0)) {
		//nni_chunk_free(&m->m_body);
		NNI_FREE_STRUCT(m);
	//}
}

// from message.c ends



int
nni_lmq_init(nni_lmq *lmq, size_t cap)
{
	size_t alloc;

	// We prefer alloc to a power of 2, this allows us to do modulo
	// operations as a power of two, for efficiency.  It does possibly
	// waste some space, but never more than 2x.  Consumers should try
	// for powers of two if they are concerned about efficiency.
	alloc = 2;
	while (alloc < cap) {
		alloc *= 2;
	}
	if ((lmq->lmq_msgs = nni_zalloc(sizeof(nng_msg *) * alloc)) == NULL) {
		NNI_FREE_STRUCT(lmq);
		return (NNG_ENOMEM);
	}
	lmq->lmq_cap   = cap;
	lmq->lmq_alloc = alloc;
	lmq->lmq_mask  = (alloc - 1);
	lmq->lmq_len   = 0;
	lmq->lmq_get   = 0;
	lmq->lmq_put   = 0;

	return (0);
}

void
nni_lmq_fini(nni_lmq *lmq)
{
	if (lmq == NULL) {
		return;
	}

	/* Free any orphaned messages. */
	while (lmq->lmq_len > 0) {
		nng_msg *msg = lmq->lmq_msgs[lmq->lmq_get++];
		lmq->lmq_get &= lmq->lmq_mask;
		lmq->lmq_len--;
		nni_msg_free(msg);
	}

	nni_free(lmq->lmq_msgs, lmq->lmq_alloc * sizeof(nng_msg *));
}

void
nni_lmq_flush(nni_lmq *lmq)
{
	while (lmq->lmq_len > 0) {
		nng_msg *msg = lmq->lmq_msgs[lmq->lmq_get++];
		lmq->lmq_get &= lmq->lmq_mask;
		lmq->lmq_len--;
		nni_msg_free(msg);
	}
}

size_t
nni_lmq_len(nni_lmq *lmq)
{
	return (lmq->lmq_len);
}

size_t
nni_lmq_cap(nni_lmq *lmq)
{
	return (lmq->lmq_cap);
}

bool
nni_lmq_full(nni_lmq *lmq)
{
	return (lmq->lmq_len >= lmq->lmq_cap);
}

bool
nni_lmq_empty(nni_lmq *lmq)
{
	return (lmq->lmq_len == 0);
}

int
nni_lmq_putq(nni_lmq *lmq, nng_msg *msg)
{
	if (lmq->lmq_len >= lmq->lmq_cap) {
		return (NNG_EAGAIN);
	}
	lmq->lmq_msgs[lmq->lmq_put++] = msg;
	lmq->lmq_len++;
	lmq->lmq_put &= lmq->lmq_mask;
	return (0);
}

int
nni_lmq_getq(nni_lmq *lmq, nng_msg **msgp)
{
	nng_msg *msg;
	if (lmq->lmq_len == 0) {
		return (NNG_EAGAIN);
	}
	msg = lmq->lmq_msgs[lmq->lmq_get++];
	lmq->lmq_get &= lmq->lmq_mask;
	lmq->lmq_len--;
	*msgp = msg;
	return (0);
}

int
nni_lmq_resize(nni_lmq *lmq, size_t cap)
{
	nng_msg * msg;
	nng_msg **newq;
	size_t    alloc;
	size_t    len;

	alloc = 2;
	while (alloc < cap) {
		alloc *= 2;
	}

	newq = nni_alloc(sizeof(nng_msg *) * alloc);
	if (newq == NULL) {
		return (NNG_ENOMEM);
	}

	len = 0;
	while ((len < cap) && (nni_lmq_getq(lmq, &msg) == 0)) {
		newq[len++] = msg;
	}

	// Flush anything left over.
	nni_lmq_flush(lmq);

	nni_free(lmq->lmq_msgs, lmq->lmq_alloc * sizeof(nng_msg *));
	lmq->lmq_msgs  = newq;
	lmq->lmq_cap   = cap;
	lmq->lmq_alloc = alloc;
	lmq->lmq_mask  = alloc - 1;
	lmq->lmq_len   = len;
	lmq->lmq_put   = len;
	lmq->lmq_get   = 0;

	return (0);
}
