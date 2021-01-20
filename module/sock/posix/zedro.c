// Zedro implementation of SPDK sockets interface
// HACK: Currently overwriting posix module. Future work: add as a separate module

#include "spdk/stdinc.h"

#if defined(__linux__)
#include <sys/epoll.h>
#include <linux/errqueue.h>
#elif defined(__FreeBSD__)
#include <sys/event.h>
#endif

#include "spdk/log.h"
#include "spdk/pipe.h"
#include "spdk/sock.h"
#include "spdk/util.h"
#include "spdk/likely.h"
#include "spdk_internal/sock.h"

#include <zedro_transport.h>

#define MAX_TMPBUF 1024
#define PORTNUMLEN 32
#define MIN_SO_RCVBUF_SIZE (2 * 1024 * 1024)
#define MIN_SO_SNDBUF_SIZE (2 * 1024 * 1024)
#define IOV_BATCH_SIZE 64

#if defined(SO_ZEROCOPY) && defined(MSG_ZEROCOPY)
#define SPDK_ZEROCOPY
#endif

struct spdk_posix_sock {
	struct spdk_sock	base;
	// int			fd;
    struct zedro_sock *zsock;
    struct zedro_listen_sock *lsock;

	uint32_t		sendmsg_idx;
	bool			zcopy;

	struct spdk_pipe	*recv_pipe;
	void			*recv_buf;
	int			recv_buf_sz;
	bool			pending_recv;

	TAILQ_ENTRY(spdk_posix_sock)	link;
};

struct spdk_posix_sock_group_impl {
	struct spdk_sock_group_impl	base;
	// int				fd;
	// TAILQ_HEAD(, spdk_posix_sock)	pending_recv;
};

static struct spdk_sock_impl_opts g_spdk_posix_sock_impl_opts = {
	.recv_buf_size = MIN_SO_RCVBUF_SIZE,
	.send_buf_size = MIN_SO_SNDBUF_SIZE
};


#define __posix_sock(sock) (struct spdk_posix_sock *)sock
#define __posix_group_impl(group) (struct spdk_posix_sock_group_impl *)group

static int
posix_sock_getaddr(struct spdk_sock *_sock, char *saddr, int slen, uint16_t *sport,
		   char *caddr, int clen, uint16_t *cport)
{
	printf("getaddr called\n");
	struct spdk_posix_sock *sock = __posix_sock(_sock);
	struct zedro_sock *zsock = sock->zsock;

    if(zsock == NULL) 
    {
        SPDK_ERRLOG("getaddr called on non-data socket\n");
        return -1;
    }

    zsock_get_addr(zsock, saddr, slen, sport, caddr, clen, cport);

	*sport = 4420;

	printf("getaddr result: %s %d %s %d\n", saddr, *sport, caddr, *cport);

	return 0;
}

static int
posix_sock_set_recvbuf(struct spdk_sock *_sock, int sz)
{
	return 0;
}

static int
posix_sock_set_sendbuf(struct spdk_sock *_sock, int sz)
{
	return 0;
}

static struct spdk_posix_sock *
posix_sock_alloc(struct zedro_sock *zsock, struct zedro_listen_sock *lsock)
{
	struct spdk_posix_sock *sock;

	sock = calloc(1, sizeof(*sock));
	if (sock == NULL) {
		SPDK_ERRLOG("sock allocation failed\n");
		return NULL;
	}

	sock->zsock = zsock;
    sock->lsock = lsock;

	return sock;
}


static struct spdk_sock *
posix_sock_listen(const char *ip, int port, struct spdk_sock_opts *opts)
{
	struct zedro_listen_sock *lsock = zsock_listen(ip, port);

    return &posix_sock_alloc(NULL, lsock)->base;
}

static struct spdk_sock *
posix_sock_connect(const char *ip, int port, __attribute__((unused)) struct spdk_sock_opts *opts)
{
    struct zedro_sock *zsock = zsock_connect(ip, port);

    return &posix_sock_alloc(zsock, NULL)->base;
}

static struct spdk_sock *
posix_sock_accept(struct spdk_sock *_sock)
{
	struct spdk_posix_sock		*sock = __posix_sock(_sock);
	struct spdk_posix_sock		*new_sock;

	assert(sock != NULL);

    if(sock->lsock == NULL) 
    {
        SPDK_ERRLOG("accept call on non-listening socket\n");
        return NULL;
    }

	struct zedro_sock *new_zsock = zsock_accept(sock->lsock);

	if(new_zsock == NULL) {
        return NULL;
    }

	new_sock = posix_sock_alloc(new_zsock, NULL);
	if (new_sock == NULL) {
		return NULL;
	}

	return &new_sock->base;
}

static int
posix_sock_close(struct spdk_sock *_sock)
{
	struct spdk_posix_sock *sock = __posix_sock(_sock);

	// Future work: cleanup zedro_sock

	free(sock);

	return 0;
}

static ssize_t _sock_writev(struct spdk_posix_sock *sock, struct iovec *iov, int iovcnt) {
    struct zedro_sock *zsock = sock->zsock;
	int rc, i;
    ssize_t write_bytes;

    write_bytes = 0;
    for(i = 0; i < iovcnt; i++)
    {
        rc = zsock_write(zsock, iov[i].iov_base, iov[i].iov_len);
        if(rc < 0 || rc < (int) iov[i].iov_len) {
            SPDK_ERRLOG("zedro sock write failed\n");
            errno = EBADF;
            return -1;
        }

        write_bytes += iov[i].iov_len;
    }

    return write_bytes;
}


static int
_sock_flush(struct spdk_sock *sock)
{
	// printf("_sock_flush called\n");
	struct spdk_posix_sock *psock = __posix_sock(sock);
	struct iovec iovs[IOV_BATCH_SIZE];
	int iovcnt;
	int retval;
	struct spdk_sock_request *req;
	int i;
	ssize_t rc;
	unsigned int offset;
	size_t len;

	/* Can't flush from within a callback or we end up with recursive calls */
	if (sock->cb_cnt > 0) {
		return 0;
	}

	/* Gather an iov */
	iovcnt = 0;
	req = TAILQ_FIRST(&sock->queued_reqs);
	while (req) {
		offset = req->internal.offset;

		for (i = 0; i < req->iovcnt; i++) {
			/* Consume any offset first */
			if (offset >= SPDK_SOCK_REQUEST_IOV(req, i)->iov_len) {
				offset -= SPDK_SOCK_REQUEST_IOV(req, i)->iov_len;
				continue;
			}

			iovs[iovcnt].iov_base = SPDK_SOCK_REQUEST_IOV(req, i)->iov_base + offset;
			iovs[iovcnt].iov_len = SPDK_SOCK_REQUEST_IOV(req, i)->iov_len - offset;
			iovcnt++;

			offset = 0;

			if (iovcnt >= IOV_BATCH_SIZE) {
				break;
			}
		}

		if (iovcnt >= IOV_BATCH_SIZE) {
			break;
		}

		req = TAILQ_NEXT(req, internal.link);
	}

	if (iovcnt == 0) {
		return 0;
	}

	/* Perform the vectored write */
	rc = _sock_writev(psock, &iovs[0], iovcnt);
	if (rc <= 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			return 0;
		}
		return rc;
	}

	// psock->sendmsg_idx++;

	/* Consume the requests that were actually written */
	req = TAILQ_FIRST(&sock->queued_reqs);
	while (req) {
		offset = req->internal.offset;

		for (i = 0; i < req->iovcnt; i++) {
			/* Advance by the offset first */
			if (offset >= SPDK_SOCK_REQUEST_IOV(req, i)->iov_len) {
				offset -= SPDK_SOCK_REQUEST_IOV(req, i)->iov_len;
				continue;
			}

			/* Calculate the remaining length of this element */
			len = SPDK_SOCK_REQUEST_IOV(req, i)->iov_len - offset;

			if (len > (size_t)rc) {
				/* This element was partially sent. */
				req->internal.offset += rc;
				return 0;
			}

			offset = 0;
			req->internal.offset += len;
			rc -= len;
		}

		/* Handled a full request. */
		spdk_sock_request_pend(sock, req);

        /* The writev syscall above isn't asynchronous,
        * so it's already done. */
        retval = spdk_sock_request_put(sock, req, 0);
        if (retval) {
            break;
        }
		

		if (rc == 0) {
			break;
		}

		req = TAILQ_FIRST(&sock->queued_reqs);
	}

	zedro_sockets_poll();

	return 0;
}

static int
posix_sock_flush(struct spdk_sock *_sock)
{
	return _sock_flush(_sock);
}

static ssize_t
posix_sock_readv(struct spdk_sock *_sock, struct iovec *iov, int iovcnt)
{
	// printf("readv called\n");
	struct spdk_posix_sock *sock = __posix_sock(_sock);
    struct zedro_sock *zsock = sock->zsock;
	int rc, i;
    ssize_t recv_bytes;

	zedro_sockets_poll();

	recv_bytes = 0;
	for (i = 0; i < iovcnt; i++) {
        rc = zsock_recv(zsock, iov[i].iov_base, iov[i].iov_len);
		if(rc > 0) {
			recv_bytes += rc;
		}
        if(rc <= 0 || rc < (int) iov[i].iov_len) {
            break;
        }
	}

    if(recv_bytes > 0) {
		printf("readv return %d\n", recv_bytes);
        return recv_bytes;
    } else {
        errno = EAGAIN;
		// printf("readv return -1\n");
        return -1;
    }
}

static ssize_t
posix_sock_recv(struct spdk_sock *sock, void *buf, size_t len)
{
	struct iovec iov[1];

	iov[0].iov_base = buf;
	iov[0].iov_len = len;

	return posix_sock_readv(sock, iov, 1);
}


static ssize_t
posix_sock_writev(struct spdk_sock *_sock, struct iovec *iov, int iovcnt)
{
	printf("writev called\n");
	struct spdk_posix_sock *sock = __posix_sock(_sock);
	int rc;

    /* In order to process a writev, we need to flush any asynchronous writes
	 * first. */
	rc = _sock_flush(_sock);
	if (rc < 0) {
		return rc;
	}

	if (!TAILQ_EMPTY(&_sock->queued_reqs)) {
		/* We weren't able to flush all requests */
		errno = EAGAIN;
		return -1;
	}

    return _sock_writev(sock, iov, iovcnt);
}

static void
posix_sock_writev_async(struct spdk_sock *sock, struct spdk_sock_request *req)
{
	printf("writev_async called\n");
	int rc;

	spdk_sock_request_queue(sock, req);

	/* If there are a sufficient number queued, just flush them out immediately. */
	if (sock->queued_iovcnt >= IOV_BATCH_SIZE) {
		rc = _sock_flush(sock);
		if (rc) {
			spdk_sock_abort_requests(sock);
		}
	}
}

static int
posix_sock_set_recvlowat(struct spdk_sock *_sock, int nbytes)
{
	return 0;
}

static bool
posix_sock_is_ipv6(struct spdk_sock *_sock)
{
	return false;
}

static bool
posix_sock_is_ipv4(struct spdk_sock *_sock)
{
	return true;
}

static bool
posix_sock_is_connected(struct spdk_sock *_sock)
{
	struct spdk_posix_sock *sock = __posix_sock(_sock);
	struct zedro_sock *zsock = sock->zsock;

    if(zsock == NULL) 
    {
        SPDK_ERRLOG("is_connected call on non-data socket\n");
        return false;
    }

    return zsock->connected;
}

static int
posix_sock_get_placement_id(struct spdk_sock *_sock, int *placement_id)
{
	return -1;
}

static struct spdk_sock_group_impl *
posix_sock_group_impl_create(void)
{
	struct spdk_posix_sock_group_impl *group_impl;

	group_impl = calloc(1, sizeof(*group_impl));
	if (group_impl == NULL) {
		SPDK_ERRLOG("group_impl allocation failed\n");
		return NULL;
	}

	return &group_impl->base;
}

static int
posix_sock_group_impl_add_sock(struct spdk_sock_group_impl *_group, struct spdk_sock *_sock)
{
	// struct spdk_posix_sock_group_impl *group = __posix_group_impl(_group);
	// struct spdk_posix_sock *sock = __posix_sock(_sock);
	// int rc;

	return 0;
}

static int
posix_sock_group_impl_remove_sock(struct spdk_sock_group_impl *_group, struct spdk_sock *_sock)
{
	// struct spdk_posix_sock_group_impl *group = __posix_group_impl(_group);
	// struct spdk_posix_sock *sock = __posix_sock(_sock);
	// int rc;


	spdk_sock_abort_requests(_sock);

	return 0;
}

static int
posix_sock_group_impl_poll(struct spdk_sock_group_impl *_group, int max_events,
			   struct spdk_sock **socks)
{
	// struct spdk_posix_sock_group_impl *group = __posix_group_impl(_group);
	struct spdk_sock *sock, *tmp;
	int num_events, rc;
	struct spdk_posix_sock *psock;
    struct zedro_sock *zsock;


	/* This must be a TAILQ_FOREACH_SAFE because while flushing,
	 * a completion callback could remove the sock from the
	 * group. */
	TAILQ_FOREACH_SAFE(sock, &_group->socks, link, tmp) {
		rc = _sock_flush(sock);
		if (rc) {
			spdk_sock_abort_requests(sock);
		}
	}

    zedro_sockets_poll();

    num_events = 0;
    // Future work: cycle list of sockets to avoid starvation
    TAILQ_FOREACH_SAFE(sock, &_group->socks, link, tmp) {
        if (num_events == max_events) {
			break;
		}

		psock = __posix_sock(sock);
        zsock = psock->zsock;
        if(zsock == NULL) {
            SPDK_ERRLOG("Non-data socket in poll group\n");
            continue;
        }

        if(zsock_has_rx_data(zsock)) {
            socks[num_events++] = &psock->base;
        }
	}

	return num_events;
}

static int
posix_sock_group_impl_close(struct spdk_sock_group_impl *_group)
{
	struct spdk_posix_sock_group_impl *group = __posix_group_impl(_group);

	free(group);
	return 0;
}

static int
posix_sock_impl_get_opts(struct spdk_sock_impl_opts *opts, size_t *len)
{
	if (!opts || !len) {
		errno = EINVAL;
		return -1;
	}

#define FIELD_OK(field) \
	offsetof(struct spdk_sock_impl_opts, field) + sizeof(opts->field) <= *len

	if (FIELD_OK(recv_buf_size)) {
		opts->recv_buf_size = g_spdk_posix_sock_impl_opts.recv_buf_size;
	}
	if (FIELD_OK(send_buf_size)) {
		opts->send_buf_size = g_spdk_posix_sock_impl_opts.send_buf_size;
	}

#undef FIELD_OK

	*len = spdk_min(*len, sizeof(g_spdk_posix_sock_impl_opts));
	return 0;
}

static int
posix_sock_impl_set_opts(const struct spdk_sock_impl_opts *opts, size_t len)
{
	if (!opts) {
		errno = EINVAL;
		return -1;
	}

#define FIELD_OK(field) \
	offsetof(struct spdk_sock_impl_opts, field) + sizeof(opts->field) <= len

	if (FIELD_OK(recv_buf_size)) {
		g_spdk_posix_sock_impl_opts.recv_buf_size = opts->recv_buf_size;
	}
	if (FIELD_OK(send_buf_size)) {
		g_spdk_posix_sock_impl_opts.send_buf_size = opts->send_buf_size;
	}

#undef FIELD_OK

	return 0;
}


static struct spdk_net_impl g_posix_net_impl = {
	.name		= "posix",
	.getaddr	= posix_sock_getaddr,
	.connect	= posix_sock_connect,
	.listen		= posix_sock_listen,
	.accept		= posix_sock_accept,
	.close		= posix_sock_close,
	.recv		= posix_sock_recv,
	.readv		= posix_sock_readv,
	.writev		= posix_sock_writev,
	.writev_async	= posix_sock_writev_async,
	.flush		= posix_sock_flush,
	.set_recvlowat	= posix_sock_set_recvlowat,
	.set_recvbuf	= posix_sock_set_recvbuf,
	.set_sendbuf	= posix_sock_set_sendbuf,
	.is_ipv6	= posix_sock_is_ipv6,
	.is_ipv4	= posix_sock_is_ipv4,
	.is_connected	= posix_sock_is_connected,
	.get_placement_id	= posix_sock_get_placement_id,
	.group_impl_create	= posix_sock_group_impl_create,
	.group_impl_add_sock	= posix_sock_group_impl_add_sock,
	.group_impl_remove_sock = posix_sock_group_impl_remove_sock,
	.group_impl_poll	= posix_sock_group_impl_poll,
	.group_impl_close	= posix_sock_group_impl_close,
	.get_opts	= posix_sock_impl_get_opts,
	.set_opts	= posix_sock_impl_set_opts,
};

SPDK_NET_IMPL_REGISTER(posix, &g_posix_net_impl, DEFAULT_SOCK_PRIORITY);
