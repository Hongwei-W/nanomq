

#include "idhash.c"
#include "lmq.c"
#include "copy.h"
#include <string.h>

void lmq_to_sock(nni_lmq *lmq, nano_sock *s);
uint32_t DJBHashn(char *str, uint32_t len);

int main(void){
    
    char cid1_str[] = "d3PQyibcSTp4HrdPtxXHKt96NLoCiRRi";
    char *cid1_ptr = cid1_str;
    struct mqtt_string cid1 = {cid1_ptr, 32,};
    
    char cid2_str[] = "3li1jBdSYenghJ9Al1EGgSd3v29CYn5z";
    char *cid2_ptr = cid2_str;
    struct mqtt_string cid2 = {cid2_ptr, 32,};

    char cid3_str[] = "rxBD6ouPhMJSa50jP";
    char *cid3_ptr = cid3_str;
    struct mqtt_string cid3 = {cid3_ptr, 17,};

    char cid4_str[] = "8yXLqJPkx9fi78fAIqhl";
    char *cid4_ptr = cid4_str;
    struct mqtt_string cid4 = {cid4_ptr, 20,};

    nano_conn_param conn1 = {cid1, 1};
    nano_conn_param conn2 = {cid2, 1};
    nano_conn_param conn3 = {cid3, 1};
    nano_conn_param conn4 = {cid4, 1};
    
    nni_msg nni_msg1 = {&conn1};
    nni_msg nni_msg2 = {&conn2};
    nni_msg nni_msg3 = {&conn3};
    nni_msg nni_msg4 = {&conn4};

    nni_lmq lmq;
    nano_sock s;

    nni_lmq_init(&lmq, 30);
    nni_id_map_init(&s.pipes, 0, 0, true);
    nni_lmq_putq(&lmq, &nni_msg1);
    nni_lmq_putq(&lmq, &nni_msg2);
    nni_lmq_putq(&lmq, &nni_msg3);
    nni_lmq_putq(&lmq, &nni_msg4);

    lmq_to_sock(&lmq, &s);

    // for debug only -- starts --
    printf("check\n");
    uint32_t key = 907266521;
    nng_msg* msg = nni_id_get(&s.pipes, key);
    if (msg == NULL){
        printf("failed\n");
    } else {
        printf("%s\n", msg->cparam->clientid.body);
    }
    // for debug only -- ends --



    
    
}

void lmq_to_sock(nni_lmq *lmq, nano_sock *s){

    while (nni_lmq_len(lmq) != 0) {
        nng_msg* new_msg;
        nni_lmq_getq(lmq, &new_msg);
        nano_conn_param *para = new_msg->cparam;
        struct mqtt_string client_id = para->clientid;

        // for debug only -- starts --
        printf("clientbody: %s\n", client_id.body);
        // for debug only -- ends --
        
        uint32_t key = DJBHashn(client_id.body, client_id.len);

        // for debug only -- starts --
        printf("its djbhashed value: [%d]\n", key);
        // for debug only -- starts --

        nni_id_set(&s->pipes, key, new_msg);
    }
    
}

uint32_t DJBHashn(char *str, uint32_t len){
    unsigned int hash = 5381;
	uint32_t i = 0;
    while (i<len){
        hash = ((hash << 5) + hash) + (*str++); /* times 33 */
		i++;
    }
    hash &= ~(1 << 31); /* strip the highest bit */
    return hash;
}

