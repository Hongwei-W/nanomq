//
// Copyright 2020 NanoMQ Team, Inc. <jaylin@emqx.io>
//
// This software is supplied under the terms of the MIT License, a
// copy of which should be located in the distribution where this
// file was obtained (LICENSE.txt).  A copy of the license may also be
// found online at https://opensource.org/licenses/MIT.
//
#include <nng.h>
#include <nanolib.h>
#include <protocol/mqtt/mqtt_parser.h>
#include <protocol/mqtt/mqtt.h>
#include "include/nanomq.h"
#include "include/sub_handler.h"

#define SUPPORT_MQTT5_0 0

uint8_t decode_sub_message(emq_work * work)
{
	uint8_t    *variable_ptr;
	uint8_t    *payload_ptr;
	int        vpos = 0; // pos in variable
	int        bpos = 0; // pos in payload

	int        len_of_varint = 0, len_of_property = 0, len_of_properties = 0;
	int        len_of_str, len_of_topic;
	nng_msg    *msg = work->msg;
	size_t     remaining_len = nng_msg_remaining_len(msg);

	const uint8_t proto_ver = conn_param_get_protover(work->cparam);
	uint8_t property_id;

	topic_node * topic_node_t, * _topic_node;
	topic_with_option * topic_option;

	// handle variable header
	variable_ptr = nng_msg_variable_ptr(msg);

	packet_subscribe * sub_pkt = work->sub_pkt;
	NNI_GET16(variable_ptr + vpos, sub_pkt->packet_id);
	vpos += 2;

#if SUPPORT_MQTT5_0
	// Only Mqtt_v5 include property. 
	if (PROTOCOL_VERSION_v5 == proto_ver) {
		// length of property in varibale
		len_of_properties = get_var_integer(variable_ptr+vpos, &len_of_varint);
		vpos += len_of_varint;

		// parse property in variable
		if (len_of_properties > 0) {
			while (1) {
				property_id = variable_ptr[vpos++];
				switch (property_id) {
					case SUBSCRIPTION_IDENTIFIER:
						sub_pkt->sub_id.varint = get_var_integer(variable_ptr+vpos, &len_of_varint);
						vpos += len_of_varint;
						break;
					case USER_PROPERTY:
						// key
						len_of_str = get_utf8_str(&(sub_pkt->user_property.strpair.str_key), variable_ptr, &vpos);
						sub_pkt->user_property.strpair.len_key = len_of_str;
						// vpos += len_of_str;

						// value
						len_of_str = get_utf8_str(&(sub_pkt->user_property.strpair.str_value), variable_ptr, &vpos);
						sub_pkt->user_property.strpair.len_value = len_of_str;
						// vpos += len_of_str;
						break;
					default:
						// avoid error
						if (vpos > remaining_len) {
							debug_msg("ERROR_IN_LEN_VPOS");
						}
				}
			}
		}
	}
#endif

	debug_msg("remainLen: [%ld] packetid : [%d]", remaining_len, sub_pkt->packet_id);
	// handle payload
	payload_ptr = nng_msg_payload_ptr(msg);

	debug_msg("V:[%x %x %x %x] P:[%x %x %x %x].",
			variable_ptr[0], variable_ptr[1], variable_ptr[2], variable_ptr[3],
			payload_ptr[0], payload_ptr[1], payload_ptr[2], payload_ptr[3]);

	if ((topic_node_t = nng_alloc(sizeof(topic_node))) == NULL) {
		debug_msg("ERROR: nng_alloc");
		return NNG_ENOMEM;
	}
	topic_node_t->next = NULL;
	sub_pkt->node = topic_node_t;

	while (1) {
		if ((topic_option = nng_alloc(sizeof(topic_with_option))) == NULL) {
			debug_msg("ERROR: nng_alloc");
			return NNG_ENOMEM;
		}
		topic_node_t->it = topic_option;
		_topic_node = topic_node_t;

		NNI_GET16(payload_ptr+bpos, len_of_topic);
		bpos += 2;

		if (len_of_topic != 0) {
			topic_option->topic_filter.len = len_of_topic;
			topic_option->topic_filter.str_body = nng_alloc(len_of_topic);
			if (topic_option->topic_filter.str_body == NULL) {
				debug_msg("ERROR: nng_alloc");
				return NNG_ENOMEM;
			}
			strncpy(topic_option->topic_filter.str_body, payload_ptr + bpos, len_of_topic);
			bpos += len_of_topic;
		} else {
			debug_msg("ERROR : topic length error.");
			return PROTOCOL_ERROR;
		}

		memcpy(topic_option, payload_ptr+bpos, 1);

		debug_msg("bpos+vpos: [%d] remainLen: [%ld].", bpos+vpos, remaining_len);
		if (++bpos < remaining_len - vpos) {
			if ((topic_node_t = nng_alloc(sizeof(topic_node))) == NULL) {
				debug_msg("ERROR: nng_alloc");
				return NNG_ENOMEM;
			}
			topic_node_t->next = NULL;
			_topic_node->next = topic_node_t;
		} else {
			break;
		}
	}
	return SUCCESS;
}

uint8_t encode_suback_message(nng_msg * msg, emq_work * work)
{
	nng_msg_clear(msg);

	uint8_t  packet_id[2];
	uint8_t  varint[4];
	uint8_t  reason_code, cmd;
	uint32_t remaining_len;
	int      len_of_varint, rv;
	topic_node * node;

	packet_subscribe *sub_pkt = work->sub_pkt;
	const uint8_t    proto_ver = conn_param_get_protover(work->cparam);

	// handle variable header first
	NNI_PUT16(packet_id, sub_pkt->packet_id);
	if ((rv = nng_msg_append(msg, packet_id, 2)) != 0) {
		debug_msg("ERROR: nng_msg_appened [%d]", rv);
		return PROTOCOL_ERROR;
	}

#if SUPPORT_MQTT5_0
	if (PROTOCOL_VERSION_v5 == proto_ver) { // add property in variable
	}
#endif

	// handle payload
	node = sub_pkt->node;
	while (node) {
		if (PROTOCOL_VERSION_v5 == proto_ver) {
		} else {
			if (node->it->reason_code == 0x80) {
				reason_code = 0x80;
			} else {
				reason_code = node->it->qos;
			}
			// MQTT_v3: 0x00-qos0  0x01-qos1  0x02-qos2  0x80-fail
			if ((rv = nng_msg_append(msg, (uint8_t *) &reason_code, 1)) != 0) {
				debug_msg("ERROR: nng_msg_append [%d]", rv);
				return PROTOCOL_ERROR;
			}
		}
		node = node->next;
		debug_msg("reason_code: [%x]", reason_code);
	}
	// handle fixed header
	cmd = CMD_SUBACK;
	if ((rv = nng_msg_header_append(msg, (uint8_t *) &cmd, 1)) != 0) {
		debug_msg("ERROR: nng_msg_header_append [%d]", rv);
		return PROTOCOL_ERROR;
	}

	remaining_len = (uint32_t)nng_msg_len(msg);
	len_of_varint = put_var_integer(varint, remaining_len);
	if ((rv = nng_msg_header_append(msg, varint, len_of_varint)) != 0) {
		debug_msg("ERROR: nng_msg_header_append [%d]", rv);
		return PROTOCOL_ERROR;
	}

	debug_msg("remain: [%d]"
		" varint: [%d %d %d %d]"
		" len: [%d]"
		" packetid: [%x %x]",
		remaining_len,
		varint[0], varint[1], varint[2], varint[3],
		len_of_varint,
		packet_id[0], packet_id[1]);

	return SUCCESS;
}

// generate ctx for each topic
uint8_t sub_ctx_handle(emq_work * work, client_ctx * cli_ctx)
{
	topic_node * topic_node_t = work->sub_pkt->node;
	char * topic_str;
	struct client * client;
	struct topic_queue * tq;

	// insert ctx_sub into treeDB
	while (topic_node_t) {
		struct topic_and_node tan;
		if ((client = nng_alloc(sizeof(struct client))) == NULL) {
			debug_msg("ERROR: nng_alloc");
			return NNG_ENOMEM;
		}

		//setting client
		client->id = (char *)conn_param_get_clentid((conn_param *)nng_msg_get_conn_param(work->msg));
		client->ctxt = cli_ctx;
		client->next = NULL;

		// setting client_ctx
		cli_ctx->pid.id = work->pid.id;
		cli_ctx->cparam = work->cparam;
		cli_ctx->sub_pkt = work->sub_pkt;

		if ((topic_str = nng_alloc(topic_node_t->it->topic_filter.len + 1)) == NULL) {
			debug_msg("ERROR: nng_alloc");
			return NNG_ENOMEM;
		}
		strncpy(topic_str, topic_node_t->it->topic_filter.str_body, topic_node_t->it->topic_filter.len);
		topic_str[topic_node_t->it->topic_filter.len] = '\0';
		debug_msg("topicLen: [%d] body: [%s]", topic_node_t->it->topic_filter.len, (char *)topic_str);

		char ** topics = topic_parse(topic_str);
		search_node(work->db, topics, &tan);

		if (tan.topic) { // not contain the node
			add_node(&tan, client);
			add_topic(client->id, topic_str);
			add_pipe_id(work->pid.id, client->id);
			// check
			tq = get_topic(client->id);
			debug_msg("-----CHECKHASHTABLE----clientid: [%s]---topic: [%s]---pipeid: [%d]",
				client->id, tq->topic, work->pid.id);
		} else {
			// not contain clientid
			if (tan.node->sub_client==NULL || check_client(tan.node, client->id)) {
				add_topic(client->id, topic_str);
				add_pipe_id(work->pid.id, client->id);
				add_client(&tan, client);
				/* check
				search_node(work->db, topics, &tan);
				struct client * cli = tan.node->sub_client;
				while(cli){
					debug_msg("client: %s", cli->id);
					cli = cli->next;
				}
				*/
			} else { // clientid already in hash
				work->sub_pkt->node->it->reason_code = 0x80;
			}
		}

		free_topic_queue(topics);
		nng_free(topic_str, topic_node_t->it->topic_filter.len+1);
		topic_node_t = topic_node_t->next;
	}

	// check treeDB
	print_db_tree(work->db);
	debug_msg("end of sub ctx handle. \n");
	return SUCCESS;
}

void del_sub_ctx(void * ctxt, char * target_topic)
{
	client_ctx * cli_ctx = ctxt;
	if (!cli_ctx) {
		debug_msg("ERROR : ctx lost!");
		return;
	}
	if (!cli_ctx->sub_pkt) {
		debug_msg("ERROR : ctx->sub is nil");
		return;
	}
	packet_subscribe * sub_pkt = cli_ctx->sub_pkt;
	if (!(sub_pkt->node)) {
		debug_msg("ERROR : not find topic");
		return;
	}

	topic_node * topic_node_t      = sub_pkt->node;
	topic_node * before_topic_node = NULL;
	while (topic_node_t) {
		if (!strncmp(topic_node_t->it->topic_filter.str_body, target_topic,
			topic_node_t->it->topic_filter.len)) {
//			debug_msg("FREE in topic_node [%s] in tree", topic_node_t->it->topic_filter.str_body);
			if (before_topic_node) {
				before_topic_node->next = topic_node_t->next;
			} else {
				sub_pkt->node = topic_node_t->next;
			}

			nng_free(topic_node_t->it->topic_filter.str_body, topic_node_t->it->topic_filter.len);
			nng_free(topic_node_t->it, sizeof(topic_with_option));
			nng_free(topic_node_t, sizeof(topic_node));
			break;
		}
		/* check
		else{
			debug_msg("a/topic b/topic [%s] [%s]", topic_node_t->it->topic_filter.str_body, target_topic);
		}
		*/
		before_topic_node = topic_node_t;
		topic_node_t = topic_node_t->next;
	}

	if (sub_pkt->node == NULL) {
		nng_free(sub_pkt, sizeof(packet_subscribe));
		// TODO free conn_param
		// debug_msg("Free--clientctx: [%p]----pipeid: [%d]", cli_ctx, cli_ctx->pid.id);
		nng_free(cli_ctx, sizeof(client_ctx));
		cli_ctx = NULL;
	}
}

void destroy_sub_ctx(void * ctxt)
{
	client_ctx * cli_ctx = ctxt;
	if (!cli_ctx) {
		debug_msg("ERROR : ctx lost!");
		return;
	}
	if (!cli_ctx->sub_pkt) {
		debug_msg("ERROR : ctx->sub is nil");
		return;
	}
	packet_subscribe * sub_pkt = cli_ctx->sub_pkt;
	if (!(sub_pkt->node)) {
		nng_free(sub_pkt, sizeof(packet_subscribe));
		nng_free(cli_ctx, sizeof(client_ctx));
		cli_ctx = NULL;
		return;
	}

	topic_node * topic_node_t = sub_pkt->node;
	topic_node * next_topic_node = NULL;
	while (topic_node_t) {
		next_topic_node = topic_node_t->next;
		nng_free(topic_node_t->it->topic_filter.str_body, topic_node_t->it->topic_filter.len);
		nng_free(topic_node_t->it, sizeof(topic_with_option));
		nng_free(topic_node_t, sizeof(topic_node));
		topic_node_t = next_topic_node;
	}

	if (sub_pkt->node == NULL) {
		nng_free(sub_pkt, sizeof(packet_subscribe));
		// TODO free conn_param
		nng_free(cli_ctx, sizeof(client_ctx));
		cli_ctx = NULL;
	}
}

void del_sub_pipe_id(uint32_t pipe_id)
{
	if (check_pipe_id(pipe_id)) {
		del_pipe_id(pipe_id);
	}
}

void del_sub_client_id(char * clientid)
{
	if (check_id(clientid)) {
		del_topic_all(clientid);
	}
}

