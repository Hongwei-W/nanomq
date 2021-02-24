#include "idhash.h"

typedef struct nano_sock nano_sock;

struct nano_sock {
	//nni_mtx        lk;
	//nni_atomic_int ttl;
	nni_id_map     pipes;
	//nni_list       recvpipes; // list of pipes with data to receive
	//nni_list       recvq;
	//nano_ctx       ctx;		//base socket
	//nni_pollable   readable;
	//nni_pollable   writable;
};