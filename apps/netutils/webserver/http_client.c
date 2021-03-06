/****************************************************************************
 *
 * Copyright 2016 Samsung Electronics All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 ****************************************************************************/

#include <fcntl.h>
#include <apps/netutils/webserver/http_err.h>
#include <apps/netutils/webserver/http_keyvalue_list.h>
#include <apps/netutils/webclient.h>
#include <apps/netutils/websocket.h>
#include <tinyara/fs/fs.h>
#include <tinyara/fs/dirent.h>

#include "http.h"
#include "http_client.h"
#include "http_string_util.h"
#include "http_query.h"
#include "http_arch.h"
#include "http_log.h"

#ifdef CONFIG_NETUTILS_WEBSOCKET
#include <apps/netutils/websocket.h>
#endif

#define MIN_WS_HEADER_FIELD 2

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static struct http_client_t *http_client_init(struct http_server_t *server, int sock_fd)
{
	struct http_client_t *p = (struct http_client_t *)HTTP_MALLOC(sizeof(struct http_client_t));

	if (p == NULL) {
		return NULL;
	}

	memset(p, 0, sizeof(struct http_client_t));

	p->client_fd = sock_fd;
	p->server = server;

	return p;
}

static int http_client_release(struct http_client_t *client)
{
#ifdef CONFIG_NET_SECURITY_TLS
	if (client->server->tls_init && client->ws_state < MIN_WS_HEADER_FIELD) {
		http_client_tls_release(client);
	}
#endif
	HTTP_FREE(client);
	HTTP_LOGD("Free Client\n");
	return HTTP_OK;
}

static int http_recv_and_handle_request(struct http_client_t *client, struct http_keyvalue_list_t *request_params)
{
	char *buf;
	int len = 0;
	char *body = NULL;
	int read_finish = false;

	int method = HTTP_METHOD_UNKNOWN;
	char url[HTTP_CONF_MAX_REQUEST_HEADER_URL_LENGTH] = { 0, };
	int buf_len = 0;
	int remain = HTTP_CONF_MAX_REQUEST_LENGTH;
	int enc = HTTP_CONTENT_LENGTH;
	struct http_req_message req = { 0, };
	int state = HTTP_REQUEST_HEADER;
	struct http_message_len_t mlen = { 0, };
	struct sockaddr_in addr;
	socklen_t addr_len;
#ifdef CONFIG_NETUTILS_WEBSOCKET
	websocket_t *ws = NULL;
#endif

	client->ws_state = 0;

	buf = HTTP_MALLOC(HTTP_CONF_MAX_REQUEST_LENGTH);
	if (buf == NULL) {
		HTTP_LOGE("Error: Fail to malloc buf\n");
		close(client->client_fd);
		return HTTP_ERROR;
	}
	HTTP_MEMSET(buf, 0, HTTP_CONF_MAX_REQUEST_LENGTH);
	if (getpeername(client->client_fd, (struct sockaddr *)&addr, &addr_len) < 0) {
		HTTP_LOGE("Error: Fail to getpeername\n");
		goto errout;
	}
	req.req_msg = buf;
	req.url = url;
	req.headers = request_params;
	req.client_ip = addr.sin_addr.s_addr;
	req.encoding = HTTP_CONTENT_LENGTH;

	while (!read_finish) {
		if (remain <= 0) {
			HTTP_LOGE("Error: Request size is too large!!\n");
			goto errout;
		}
#ifdef CONFIG_NET_SECURITY_TLS
		if (client->server->tls_init) {
			len = mbedtls_ssl_read(&(client->tls_ssl), (unsigned char *)buf + buf_len, HTTP_CONF_MAX_REQUEST_LENGTH - buf_len);
		} else
#endif
		{
			len = recv(client->client_fd, buf + buf_len, HTTP_CONF_MAX_REQUEST_LENGTH - buf_len, 0);
		}
		if (len < 0) {
			HTTP_LOGE("Error: Receive Fail %d\n", len);
			goto errout;
		} else if (len == 0) {
			HTTP_LOGD("Finish read\n");
			goto errout;
		}
		buf_len += len;
		remain -= len;

		read_finish = http_parse_message(buf, buf_len, &method, url, &body, &enc, &state, &mlen, request_params, client, NULL, &req);
		if (read_finish == HTTP_ERROR) {
			goto errout;
		}
	}
	if (method == HTTP_METHOD_UNKNOWN) {
		goto errout;
	}

	if (enc == HTTP_CONTENT_LENGTH) {
		req.entity = body;
		http_dispatch_url(client, &req);
	}
#ifdef CONFIG_NETUTILS_WEBSOCKET
	/* open websocket */
	if (client->ws_state >= MIN_WS_HEADER_FIELD) {
		ws = websocket_find_table();
		if (ws == NULL) {
			goto errout;
		}
		ws->fd = client->client_fd;
		ws->cb = &client->server->ws_cb;
#ifdef CONFIG_NET_SECURITY_TLS
		if (client->server->tls_init) {
			ws->tls_enabled = 1;
			ws->tls_conf = NULL;
			ws->tls_ssl = (tls_session *)malloc(sizeof(tls_session));
			if (ws->tls_ssl == NULL) {
				goto errout;
			}
			ws->tls_ssl->ssl = (mbedtls_ssl_context *)malloc(sizeof(mbedtls_ssl_context));
			if (ws->tls_ssl->ssl == NULL) {
				free(ws->tls_ssl);
				goto errout;
			}
			ws->tls_ssl->net.fd = client->tls_client_fd.fd;
			memcpy(ws->tls_ssl->ssl, &client->tls_ssl, sizeof(mbedtls_ssl_context));
			mbedtls_ssl_set_bio(ws->tls_ssl->ssl, &ws->tls_ssl->net, mbedtls_net_send, mbedtls_net_recv, NULL);
		}
#endif
		if (pthread_attr_init(&ws->thread_attr) != 0) {
			HTTP_LOGE("Error: Cannot initialize thread attribute\n");
			goto errout;
		}
		pthread_attr_setstacksize(&ws->thread_attr, WEBSOCKET_STACKSIZE);
		pthread_attr_setschedpolicy(&ws->thread_attr, SCHED_RR);
		if (pthread_create(&ws->thread_id, &ws->thread_attr, (pthread_startroutine_t) websocket_server_init, (pthread_addr_t) ws) != 0) {
			HTTP_LOGE("Error: Cannot create websocket thread!!\n");
			goto errout;
		}
		pthread_setname_np(ws->thread_id, "websocket handle server");
		pthread_detach(ws->thread_id);
	} else
#endif
	{
		close(client->client_fd);
	}

	HTTP_FREE(buf);
	if (enc == HTTP_CHUNKED_ENCODING) {
		HTTP_FREE(body);
	}
	return HTTP_OK;
errout:
#ifdef CONFIG_NETUTILS_WEBSOCKET
	if (ws) {
		TLSSession_free(ws->tls_ssl);
	}
#endif
	close(client->client_fd);
	HTTP_FREE(buf);
	if (enc == HTTP_CHUNKED_ENCODING) {
		HTTP_FREE(body);
	}
	return HTTP_ERROR;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

pthread_addr_t http_handle_client(pthread_addr_t arg)
{
	struct http_server_t *server = (struct http_server_t *)arg;
	struct http_msg_t msg;
	int result;
	struct http_keyvalue_list_t request_params;
	int sock_fd;
	struct mallinfo data;
	struct http_client_t *p;
	mqd_t msg_q;
	struct mq_attr mqattr;

	if ((msg_q = http_server_mq_open(server->port)) == NULL) {
		HTTP_LOGE("msg queue open fail in http_handle_client\n");
		return NULL;
	}

	mq_getattr(msg_q, &mqattr);

	while (1) {
		if (mq_receive(msg_q, (char *)&msg, mqattr.mq_msgsize, NULL) < 0) {
			return NULL;
		}

		sock_fd = msg.data;

		if (msg.event == HTTP_STOP_EVENT) {
			close(sock_fd);
			break;
		}

		data = mallinfo();

		if (data.fordblks < HTTP_CONF_MIN_TLS_MEMORY * HTTP_CONF_MAX_CLIENT_HANDLE) {
			HTTP_LOGE("Error: Not enough memory :: %d\n", data.fordblks);
			close(sock_fd);
			continue;

		}

		HTTP_LOGD("Free Mem %d\n", data.fordblks);

		p = http_client_init(server, sock_fd);

		if (p == NULL) {
			HTTP_LOGE("Error: Cannot init client!!\n");
			close(sock_fd);
			continue;
		}

		HTTP_LOGD("Client %d.\n", p->client_fd);

#ifdef CONFIG_NET_SECURITY_TLS
		if (server->tls_init) {
			if (http_client_tls_init(p) != HTTP_OK) {
				HTTP_LOGE("Error: Cannot initialize TLS!! Close client.. %d\n", sock_fd);
				http_client_release(p);
				continue;
			}
		}
#endif
		http_keyvalue_list_init(&request_params);
		result = http_recv_and_handle_request(p, &request_params);
		http_keyvalue_list_release(&request_params);

		if (result != HTTP_OK) {
			HTTP_LOGD("Client %d  in error case.\n", sock_fd);
		} else {
			HTTP_LOGD("Client %d  in normal case.\n", sock_fd);
		}
		http_client_release(p);
		HTTP_LOGD("Release client....\n");
	}

	mq_close(msg_q);

	HTTP_LOGD("Closed client handle %d\n", getpid());
	return NULL;
}

int http_parse_message(char *buf, int buf_len, int *method, char *url, char **body, int *enc, int *state, struct http_message_len_t *len, struct http_keyvalue_list_t *params, struct http_client_t *client, struct http_client_response_t *response, struct http_req_message *req)
{
	int ret = 0;
	int protocol = 0;
	int sentence_end = 0;
	char key[HTTP_CONF_MAX_KEY_LENGTH] = { 0, };
	char value[HTTP_CONF_MAX_VALUE_LENGTH] = { 0, };
	int process_finish = false;
	int read_finish = false;
	char *entity = *body;

	while (!process_finish) {
		/* At this point, we read a line of http request */
		switch (*state) {
		case HTTP_REQUEST_HEADER:
			len->sentence_start = 0;
			len->message_len = 0;
			/* Search CR and LF */
			sentence_end = http_find_first_crlf(buf, buf_len, len->sentence_start);
			if (sentence_end != -1) {
				buf[sentence_end] = '\0';

				/* If this is called by webserver */
				if (client) {
					/* Header Examples
					 * ---------------
					 * 1. GET / HTTP/1.1
					 * 2. GET /cgi-bin/http_trace.pl HTTP/1.1\r\n
					 */
					if ((ret = http_separate_header(buf + len->sentence_start, method, url, &protocol)) != HTTP_OK) {
						HTTP_LOGD("Fail to separate header %d\n", ret);
						return HTTP_ERROR;
					}

					HTTP_LOGD("Request Method : %s\n", (*method == HTTP_METHOD_GET) ? "GET" : (*method == HTTP_METHOD_PUT) ? "PUT" : (*method == HTTP_METHOD_POST) ? "POST" : (*method == HTTP_METHOD_DELETE) ? "DELETE" : "UNKNOWN");
					req->method = *method;
					HTTP_LOGD("Request URI : %s\n", url);
					HTTP_LOGD("Request Protocol : ");
					if (protocol == HTTP_HTTP_VERSION_09) {
						HTTP_LOGD("HTTP/0.9\n");
					} else if (protocol == HTTP_HTTP_VERSION_10) {
						HTTP_LOGD("HTTP/1.0\n");
					} else if (protocol == HTTP_HTTP_VERSION_11) {
						HTTP_LOGD("HTTP/1.1\n");
					} else {
						HTTP_LOGD("unknown version\n");
					}
				} else {		/* If this is called by webclient */
					if ((ret = http_separate_status_line(buf + len->sentence_start, &protocol, &response->status, response->phrase)) != HTTP_OK) {
						HTTP_LOGD("Fail to separate status line %d\n", ret);
						return HTTP_ERROR;
					}
					HTTP_LOGD("Response Status : %d\n", response->status);
					HTTP_LOGD("Response Phrase : %s\n", response->phrase);
				}
				len->sentence_start = sentence_end + 2;
				*state = HTTP_REQUEST_PARAMETERS;
			} else {
				read_finish = true;
				process_finish = true;
			}
			break;
		case HTTP_REQUEST_PARAMETERS:
			/* Search CR and LF */
			sentence_end = http_find_first_crlf(buf, buf_len, len->sentence_start);
			if (sentence_end >= 0) {
				buf[sentence_end] = '\0';
				if (strlen(buf + len->sentence_start) > 0) {
					/* Read parameters */
					if ((ret = http_separate_keyvalue(buf + len->sentence_start, key, value)) != HTTP_OK) {
						HTTP_LOGD("Fail to separate keyvalue %d\n", ret);
						return HTTP_ERROR;
					}
					HTTP_LOGD("[HTTP Parameter] Key: %s / Value: %s\n", key, value);

					http_keyvalue_list_add(params, key, value);
					if (client) {
						if (strcmp(key, "Connection") == 0 && strcmp(value, "Upgrade") == 0) {
							++client->ws_state;
						}
						if (strcmp(key, "Upgrade") == 0 && strcmp(value, "websocket") == 0) {
							++client->ws_state;
						}
						if (strcmp(key, "Sec-WebSocket-Key") == 0) {
#ifdef CONFIG_NETUTILS_WEBSOCKET
							strncpy((char *)client->ws_key, value, WEBSOCKET_CLIENT_KEY_LEN);
#else
							HTTP_LOGE("Websocket is not supported\n");
#endif
						}
					}
					if (strcmp(key, "Content-Length") == 0) {
						len->content_len = HTTP_ATOI(value);

						HTTP_LOGD("This request contains contents, length : %d\n", len->content_len);
					}
					if (strcmp(key, "Transfer-Encoding") == 0 && strcmp(value, "chunked") == 0) {
						if (client) {
							len->chunked_remain = 0;
							len->entity_len = 0;
							entity = HTTP_MALLOC(HTTP_CONF_MAX_ENTITY_LENGTH + 1);
							if (entity == NULL) {
								HTTP_LOGE("Error: Fail to alloc memory\n");
								return HTTP_ERROR;
							}
							*enc = HTTP_CHUNKED_ENCODING;
							req->encoding = *enc;
							*body = entity;
							HTTP_LOGD("This request contains chunked encoding contents\n");
						} else {
							HTTP_LOGD("Weblient cannot support chunked encoding.\n");
						}
					}
				} else {
					*state = HTTP_REQUEST_BODY;
				}
				len->sentence_start = sentence_end + 2;
			} else {
				process_finish = true;
			}
			break;

		case HTTP_REQUEST_BODY:
			if (client && *method != HTTP_METHOD_POST && *method != HTTP_METHOD_PUT) {
				read_finish = true;
				return read_finish;
			}
			/* Content length */
			if (len->sentence_start < 0 || sentence_end < 0) {
				HTTP_LOGE("Error: Wrong sentence arrange\n");
				break;
			}
			if (*enc == HTTP_CONTENT_LENGTH) {
				*body = buf + len->sentence_start;
				if (buf_len - len->sentence_start + len->message_len >= len->content_len) {
					if (len->entity_len == 0) {
						len->entity_len += (buf_len - len->sentence_start);
					} else {
						len->entity_len += buf_len;
					}
					len->message_len += (buf_len - len->sentence_start);
					HTTP_LOGD("==== read finish ====\n");
					HTTP_LOGD("All body read\n");
					read_finish = true;
				} else {
					if (len->entity_len == 0) {
						len->entity_len += (buf_len - len->sentence_start);
						len->message_len += (buf_len - len->sentence_start);
					} else {
						len->entity_len += buf_len;
						len->message_len += buf_len;
					}
					HTTP_LOGD("Not all body read\n");
				}

				HTTP_LOGD("BODY : entity_len[%d]\n", len->entity_len);
				HTTP_LOGD("BODY : buf_len[%d]\n", buf_len);
				HTTP_LOGD("BODY : len->sentence_start[%d]\n", len->sentence_start);
				HTTP_LOGD("BODY : len->message_len[%d]\n", len->message_len);
				HTTP_LOGD("BODY : len->content_len[%d]\n", len->content_len);

				process_finish = true;
			}
			/* Chunked encoding */
			else {
				int i, j, cha;
				if (!len->chunked_remain) {
					len->content_len = 0;
					sentence_end = http_find_first_crlf(buf, buf_len, len->sentence_start);

					if (sentence_end < 0) {
						HTTP_LOGE("Error: Cannot find body\n");
						process_finish = true;
						read_finish = true;
						break;
					}

					/* calculate chunked size */
					for (i = 1; sentence_end - i >= len->sentence_start; ++i) {
						cha = (int) * (buf + sentence_end - i);
						if (cha > '9') {
							cha = cha - 'a' + 10;
						} else {
							cha = cha - '0';
						}
						for (j = 1; j < i; ++j) {
							cha *= 16;
						}
						len->content_len += cha;
					}
					len->chunked_remain = len->content_len;
					if (!len->chunked_remain) {
						HTTP_LOGD("Chunked size is zero\n");
						entity[len->entity_len] = '\0';
						req->entity = entity + len->entity_len;
						http_dispatch_url(client, req);
						read_finish = true;
						return read_finish;
					}
					len->sentence_start = sentence_end + 2;
				}
				i = 0;
				if (len->chunked_remain > 0) {

					/* Check whether incoming entity is smaller than entity max size or not */
					if (len->entity_len + len->chunked_remain > HTTP_CONF_MAX_ENTITY_LENGTH) {
						HTTP_LOGE("Error: Incoming entity is too big\n");
						return HTTP_ERROR;
					}

					for (i = 0; len->chunked_remain > 0; ++i) {
						if (len->sentence_start >= buf_len + len->message_len) {
							len->message_len = len->sentence_start;
							len->entity_len += i;
							read_finish = false;
							return read_finish;
						}
						entity[i + len->entity_len] = *(buf + len->sentence_start++);
						--len->chunked_remain;
					}
					len->entity_len += i;
				}

				if (!len->chunked_remain) {
					entity[len->entity_len] = '\0';
					req->entity = entity + len->entity_len - len->content_len;
					http_dispatch_url(client, req);
					len->sentence_start += 2;
					sentence_end = http_find_first_crlf(buf, buf_len + len->message_len, len->sentence_start);
					if (sentence_end < 0) {
						HTTP_LOGE("Error: Not accord with chunked encoding\n");
						read_finish = true;
						return read_finish;
					}
					len->sentence_start = sentence_end + 2;
					if (*(buf + sentence_end - 1) == '0') {
						req->entity = entity + len->entity_len;
						http_dispatch_url(client, req);
						process_finish = true;
						read_finish = true;
					}
				}
			}
			break;
		}
	}
	return read_finish;
}

int http_send_response(struct http_client_t *client, int status, const char *body, struct http_keyvalue_list_t *headers)
{
	char *buf;
	int buflen = 0, ret, sndlen;
	struct http_keyvalue_t *cur = NULL;

	buf = HTTP_MALLOC(HTTP_CONF_MAX_REQUEST_LENGTH);
	if (buf == NULL) {
		HTTP_LOGE("Error: Fail to malloc buffer\n");
		return HTTP_ERROR;
	}
#ifdef CONFIG_NETUTILS_WEBSOCKET
	if (client->ws_state >= MIN_WS_HEADER_FIELD) {
		unsigned char accept_key[WEBSOCKET_ACCEPT_KEY_LEN] = { 0, };
		websocket_create_accept_key(accept_key, WEBSOCKET_ACCEPT_KEY_LEN, client->ws_key, WEBSOCKET_CLIENT_KEY_LEN);
		buflen = snprintf(buf, HTTP_CONF_MAX_REQUEST_LENGTH, "HTTP/1.1 101 Switching Protocols\r\n" "Upgrade: websocket\r\n" "Connection: Upgrade\r\n" "Sec-WebSocket-Accept: %s\r\n\r\n", accept_key);
	} else
#endif
	{
		buflen = snprintf(buf, HTTP_CONF_MAX_REQUEST_LENGTH, "HTTP/1.1 %d %s\r\n", status, (status == 200) ? "OK" : "NOT OK");
		if (headers) {
			cur = headers->head->next;
			while (cur != headers->tail) {
				buflen += snprintf(buf + buflen, HTTP_CONF_MAX_REQUEST_LENGTH - buflen, "%s: %s\r\n", cur->key, cur->value);
				cur = cur->next;
			}
		}

		if (headers == NULL) {
			buflen += snprintf(buf + buflen, HTTP_CONF_MAX_REQUEST_LENGTH - buflen, "Content-type: text/html\r\n" "Connection: close\r\n");
			if (body) {
				snprintf(buf + buflen, HTTP_CONF_MAX_REQUEST_LENGTH - buflen, "Content-Length: %d\r\n" "\r\n" "%s", strlen(body), body);
			} else {
				snprintf(buf + buflen, HTTP_CONF_MAX_REQUEST_LENGTH - buflen, "\r\n");
			}
		} else {
			snprintf(buf + buflen, HTTP_CONF_MAX_REQUEST_LENGTH - buflen, "\r\n%s", body);
		}
	}

	sndlen = strlen(buf);
	buflen = 0;
	while (sndlen > 0) {
#ifdef CONFIG_NET_SECURITY_TLS
		if (client->server->tls_init) {
			ret = mbedtls_ssl_write(&(client->tls_ssl), (unsigned char *)buf + buflen, sndlen);
		} else
#endif
		{
			ret = send(client->client_fd, buf + buflen, sndlen, 0);
		}

		if (ret < 1) {
			HTTP_FREE(buf);
			return HTTP_ERROR;
		} else {
			sndlen -= ret;
			buflen += ret;
		}
	}
	HTTP_FREE(buf);
	return HTTP_OK;
}
