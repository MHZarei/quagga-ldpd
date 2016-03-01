/*	$OpenBSD$ */

/*
 * Copyright (c) 2013, 2016 Renato Westphal <renato@openbsd.org>
 * Copyright (c) 2009 Michele Marchetto <michele@openbsd.org>
 * Copyright (c) 2004, 2005, 2008 Esben Norby <norby@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <zebra.h>

#include "ldpd.h"
#include "ldpe.h"
#include "log.h"

#include "sockopt.h"

static struct iface		*disc_find_iface(unsigned int, int,
				    union ldpd_addr *, int);
static int			 session_read(struct thread *);
static int			 session_write(struct thread *);
static ssize_t			 session_get_pdu(struct ibuf_read *, char **);
static void			 tcp_close(struct tcp_conn *);
static struct pending_conn	*pending_conn_new(int, int, union ldpd_addr *);
static int			 pending_conn_timeout(struct thread *);

int
gen_ldp_hdr(struct ibuf *buf, uint16_t size)
{
	struct ldp_hdr	ldp_hdr;

	memset(&ldp_hdr, 0, sizeof(ldp_hdr));
	ldp_hdr.version = htons(LDP_VERSION);
	/* exclude the 'Version' and 'PDU Length' fields from the total */
	ldp_hdr.length = htons(size - LDP_HDR_DEAD_LEN);
	ldp_hdr.lsr_id = ldp_rtr_id_get(leconf);
	ldp_hdr.lspace_id = 0;

	return (ibuf_add(buf, &ldp_hdr, LDP_HDR_SIZE));
}

int
gen_msg_hdr(struct ibuf *buf, uint32_t type, uint16_t size)
{
	static int	msgcnt = 0;
	struct ldp_msg	msg;

	memset(&msg, 0, sizeof(msg));
	msg.type = htons(type);
	/* exclude the 'Type' and 'Length' fields from the total */
	msg.length = htons(size - LDP_MSG_DEAD_LEN);
	msg.id = htonl(++msgcnt);

	return (ibuf_add(buf, &msg, sizeof(msg)));
}

/* send packets */
int
send_packet(int fd, int af, union ldpd_addr *dst, struct iface_af *ia,
    void *pkt, size_t len)
{
	struct sockaddr		*sa;

	switch (af) {
	case AF_INET:
		if (ia && IN_MULTICAST(ntohl(dst->v4.s_addr))) {
			/* set outgoing interface for multicast traffic */
			if (sock_set_ipv4_mcast(ia->iface) == -1) {
				log_debug("%s: error setting multicast "
				    "interface, %s", __func__, ia->iface->name);
				return (-1);
			}
		}
		break;
	case AF_INET6:
		if (ia && IN6_IS_ADDR_MULTICAST(&dst->v6)) {
			/* set outgoing interface for multicast traffic */
			if (sock_set_ipv6_mcast(ia->iface) == -1) {
				log_debug("%s: error setting multicast "
				    "interface, %s", __func__, ia->iface->name);
				return (-1);
			}
		}
		break;
	default:
		fatalx("send_packet: unknown af");
	}

	sa = addr2sa(af, dst, LDP_PORT);
	if (sendto(fd, pkt, len, 0, sa, sockaddr_len(sa)) == -1) {
		log_warn("%s: error sending packet to %s", __func__,
		    log_sockaddr(sa));
		return (-1);
	}

	return (0);
}

/* Discovery functions */
int
disc_recv_packet(struct thread *thread)
{
	int			 fd = THREAD_FD(thread);
	struct thread		**threadp = THREAD_ARG(thread);

	union {
		struct	cmsghdr hdr;
#ifdef HAVE_STRUCT_SOCKADDR_DL
		char	buf[CMSG_SPACE(sizeof(struct sockaddr_dl))];
#else
		char	buf[CMSG_SPACE(sizeof(struct in6_pktinfo))];
#endif
	} cmsgbuf;
	struct msghdr		 m;
	struct sockaddr_storage	 from;
	struct iovec		 iov;
	char			*buf;
#ifndef MSG_MCAST
	struct cmsghdr		*cmsg;
#endif
	ssize_t			 r;
	int			 multicast;
	int			 af;
	union ldpd_addr		 src;
	unsigned int		 ifindex = 0;
	struct iface		*iface;
	uint16_t		 len;
	struct ldp_hdr		 ldp_hdr;
	uint16_t		 pdu_len;
	struct ldp_msg		 msg;
	uint16_t		 msg_len;
	struct in_addr		 lsr_id;

	/* reschedule read */
	*threadp = thread_add_read(master, disc_recv_packet, threadp, fd);

	/* setup buffer */
	memset(&m, 0, sizeof(m));
	iov.iov_base = buf = pkt_ptr;
	iov.iov_len = IBUF_READ_SIZE;
	m.msg_name = &from;
	m.msg_namelen = sizeof(from);
	m.msg_iov = &iov;
	m.msg_iovlen = 1;
	m.msg_control = &cmsgbuf.buf;
	m.msg_controllen = sizeof(cmsgbuf.buf);

	if ((r = recvmsg(fd, &m, 0)) == -1) {
		if (errno != EAGAIN && errno != EINTR)
			log_debug("%s: read error: %s", __func__,
			    strerror(errno));
		return (0);
	}

	sa2addr((struct sockaddr *)&from, &af, &src, NULL);
#ifdef MSG_MCAST
	multicast = (m.msg_flags & MSG_MCAST) ? 1 : 0;
#else
	multicast = 0;
	for (cmsg = CMSG_FIRSTHDR(&m); cmsg != NULL;
	    cmsg = CMSG_NXTHDR(&m, cmsg)) {
#if defined(HAVE_IP_PKTINFO)
		if (af == AF_INET && cmsg->cmsg_level == IPPROTO_IP &&
		    cmsg->cmsg_type == IP_PKTINFO) {
			struct in_pktinfo	*pktinfo;

			pktinfo = (struct in_pktinfo *)CMSG_DATA(cmsg);
			if (IN_MULTICAST(ntohl(pktinfo->ipi_addr.s_addr)))
				multicast = 1;
			break;
		}
#elif defined(HAVE_IP_RECVDSTADDR)
		if (af == AF_INET && cmsg->cmsg_level == IPPROTO_IP &&
		    cmsg->cmsg_type == IP_RECVDSTADDR) {
			struct in_addr		*addr;

			addr = (struct in_addr *)CMSG_DATA(cmsg);
			if (IN_MULTICAST(ntohl(addr->s_addr)))
				multicast = 1;
			break;
		}
#else
#error "Unsupported socket API"
#endif
		if (af == AF_INET6 && cmsg->cmsg_level == IPPROTO_IPV6 &&
		    cmsg->cmsg_type == IPV6_PKTINFO) {
			struct in6_pktinfo	*pktinfo;

			pktinfo = (struct in6_pktinfo *)CMSG_DATA(cmsg);
			if (IN6_IS_ADDR_MULTICAST(&pktinfo->ipi6_addr))
				multicast = 1;
			break;
		}
	}
#endif /* MSG_MCAST */
	if (bad_addr(af, &src)) {
		log_debug("%s: invalid source address: %s", __func__,
		    log_addr(af, &src));
		return (0);
	}
	ifindex = getsockopt_ifindex(af, &m);

	/* find a matching interface */
	iface = disc_find_iface(ifindex, af, &src, multicast);
	if (iface == NULL)
		return (0);

	/* check packet size */
	len = (uint16_t)r;
	if (len < (LDP_HDR_SIZE + LDP_MSG_SIZE) || len > LDP_MAX_LEN) {
		log_debug("%s: bad packet size, source %s", __func__,
		    log_addr(af, &src));
		return (0);
	}

	/* LDP header sanity checks */
	memcpy(&ldp_hdr, buf, sizeof(ldp_hdr));
	if (ntohs(ldp_hdr.version) != LDP_VERSION) {
		log_debug("%s: invalid LDP version %d, source %s", __func__,
		    ntohs(ldp_hdr.version), log_addr(af, &src));
		return (0);
	}
	if (ntohs(ldp_hdr.lspace_id) != 0) {
		log_debug("%s: invalid label space %u, source %s", __func__,
		    ntohs(ldp_hdr.lspace_id), log_addr(af, &src));
		return (0);
	}
	/* check "PDU Length" field */
	pdu_len = ntohs(ldp_hdr.length);
	if ((pdu_len < (LDP_HDR_PDU_LEN + LDP_MSG_SIZE)) ||
	    (pdu_len > (len - LDP_HDR_DEAD_LEN))) {
		log_debug("%s: invalid LDP packet length %u, source %s",
		    __func__, ntohs(ldp_hdr.length), log_addr(af, &src));
		return (0);
	}
	buf += LDP_HDR_SIZE;
	len -= LDP_HDR_SIZE;

	lsr_id.s_addr = ldp_hdr.lsr_id;

	/*
	 * For UDP, we process only the first message of each packet. This does
	 * not impose any restrictions since LDP uses UDP only for sending Hello
	 * packets.
	 */
	memcpy(&msg, buf, sizeof(msg));

	/* check "Message Length" field */
	msg_len = ntohs(msg.length);
	if (msg_len < LDP_MSG_LEN || ((msg_len + LDP_MSG_DEAD_LEN) > pdu_len)) {
		log_debug("%s: invalid LDP message length %u, source %s",
		    __func__, ntohs(msg.length), log_addr(af, &src));
		return (0);
	}
	buf += LDP_MSG_SIZE;
	len -= LDP_MSG_SIZE;

	/* switch LDP packet type */
	switch (ntohs(msg.type)) {
	case MSG_TYPE_HELLO:
		recv_hello(lsr_id, &msg, af, &src, iface, multicast, buf, len);
		break;
	default:
		log_debug("%s: unknown LDP packet type, source %s", __func__,
		    log_addr(af, &src));
	}

	return (0);
}

static struct iface *
disc_find_iface(unsigned int ifindex, int af, union ldpd_addr *src,
    int multicast)
{
	struct iface	*iface;
	struct iface_af	*ia;
	struct if_addr	*if_addr;
	in_addr_t	 mask;

	iface = if_lookup(leconf, ifindex);
	if (iface == NULL)
		return (NULL);

	/*
	 * For unicast packets, we just need to make sure that the interface
	 * is enabled for the given address-family.
	 */
	if (!multicast) {
		ia = iface_af_get(iface, af);
		if (ia->enabled)
			return (iface);
		return (NULL);
	}

	switch (af) {
	case AF_INET:
		LIST_FOREACH(if_addr, &iface->addr_list, entry) {
			if (if_addr->af != AF_INET)
				continue;

			switch (iface->type) {
			case IF_TYPE_POINTOPOINT:
				if (if_addr->dstbrd.v4.s_addr == src->v4.s_addr)
					return (iface);
				break;
			default:
				mask = prefixlen2mask(if_addr->prefixlen);
				if ((if_addr->addr.v4.s_addr & mask) ==
				    (src->v4.s_addr & mask))
					return (iface);
				break;
			}
		}
		break;
	case AF_INET6:
		if (IN6_IS_ADDR_LINKLOCAL(&src->v6))
			return (iface);
		break;
	default:
		fatalx("disc_find_iface: unknown af");
	}

	return (NULL);
}

int
session_accept(struct thread *thread)
{
	int			 fd = THREAD_FD(thread);
	struct sockaddr_storage	 src;
	socklen_t		 len = sizeof(src);
	int			 newfd;
	int			 af;
	union ldpd_addr		 addr;
	struct nbr		*nbr;
	struct pending_conn	*pconn;

	newfd = accept(fd, (struct sockaddr *)&src, &len);
	if (newfd == -1) {
		/*
		 * Pause accept if we are out of file descriptors, or
		 * libevent will haunt us here too.
		 */
		if (errno == ENFILE || errno == EMFILE) {
			accept_pause();
		} else if (errno != EWOULDBLOCK && errno != EINTR &&
		    errno != ECONNABORTED)
			log_debug("%s: accept error: %s", __func__,
			    strerror(errno));
		return (0);
	}
	sock_set_nonblock(newfd);

	sa2addr((struct sockaddr *)&src, &af, &addr, NULL);

	/*
	 * Since we don't support label spaces, we can identify this neighbor
	 * just by its source address. This way we don't need to wait for its
	 * Initialization message to know who we are talking to.
	 */
	nbr = nbr_find_addr(af, &addr);
	if (nbr == NULL) {
		/*
		 * According to RFC 5036, we would need to send a No Hello
		 * Error Notification message and close this TCP connection
		 * right now. But doing so would trigger the backoff exponential
		 * timer in the remote peer, which would considerably slow down
		 * the session establishment process. The trick here is to wait
		 * five seconds before sending the Notification Message. There's
		 * a good chance that the remote peer will send us a Hello
		 * message within this interval, so it's worth waiting before
		 * taking a more drastic measure.
		 */
		pconn = pending_conn_find(af, &addr);
		if (pconn)
			close(newfd);
		else
			pending_conn_new(newfd, af, &addr);
		return (0);
	}
	/* protection against buggy implementations */
	if (nbr_session_active_role(nbr)) {
		close(newfd);
		return (0);
	}
	if (nbr->state != NBR_STA_PRESENT) {
		log_debug("%s: lsr-id %s: rejecting additional transport "
		    "connection", __func__, inet_ntoa(nbr->id));
		close(newfd);
		return (0);
	}

	session_accept_nbr(nbr, newfd);

	return (0);
}

void
session_accept_nbr(struct nbr *nbr, int fd)
{
#ifdef __OpenBSD__
	struct nbr_params	*nbrp;
	int			 opt;
	socklen_t		 len;

	nbrp = nbr_params_find(leconf, nbr->id);
	if (nbr_gtsm_check(fd, nbr, nbrp)) {
		close(fd);
		return;
	}

	if (nbrp && nbrp->auth.method == AUTH_MD5SIG) {
		if (sysdep.no_pfkey || sysdep.no_md5sig) {
			log_warnx("md5sig configured but not available");
			close(fd);
			return;
		}

		len = sizeof(opt);
		if (getsockopt(fd, IPPROTO_TCP, TCP_MD5SIG, &opt, &len) == -1)
			fatal("getsockopt TCP_MD5SIG");
		if (!opt) {	/* non-md5'd connection! */
			log_warnx("connection attempt without md5 signature");
			close(fd);
			return;
		}
	}
#endif

	nbr->tcp = tcp_new(fd, nbr);
	nbr_fsm(nbr, NBR_EVT_MATCH_ADJ);
}

static int
session_read(struct thread *thread)
{
	int		 fd = THREAD_FD(thread);
	struct nbr	*nbr = THREAD_ARG(thread);
	struct tcp_conn	*tcp = nbr->tcp;
	struct ldp_hdr	*ldp_hdr;
	struct ldp_msg	*msg;
	char		*buf = NULL, *pdu;
	ssize_t		 n, len;
	uint16_t	 pdu_len, msg_len, msg_size, max_pdu_len;
	int		 ret;

	tcp->rev = thread_add_read(master, session_read, nbr, fd);

	if ((n = read(fd, tcp->rbuf->buf + tcp->rbuf->wpos,
	    sizeof(tcp->rbuf->buf) - tcp->rbuf->wpos)) == -1) {
		if (errno != EINTR && errno != EAGAIN) {
			log_warn("%s: read error", __func__);
			nbr_fsm(nbr, NBR_EVT_CLOSE_SESSION);
			return (0);
		}
		/* retry read */
		return (0);
	}
	if (n == 0) {
		/* connection closed */
		log_debug("%s: connection closed by remote end", __func__);
		nbr_fsm(nbr, NBR_EVT_CLOSE_SESSION);
		return (0);
	}
	tcp->rbuf->wpos += n;

	while ((len = session_get_pdu(tcp->rbuf, &buf)) > 0) {
		pdu = buf;
		ldp_hdr = (struct ldp_hdr *)pdu;
		if (ntohs(ldp_hdr->version) != LDP_VERSION) {
			session_shutdown(nbr, S_BAD_PROTO_VER, 0, 0);
			free(buf);
			return (0);
		}

		pdu_len = ntohs(ldp_hdr->length);
		/*
	 	 * RFC 5036 - Section 3.5.3:
		 * "Prior to completion of the negotiation, the maximum
		 * allowable length is 4096 bytes".
		 */
		if (nbr->state == NBR_STA_OPER)
			max_pdu_len = nbr->max_pdu_len;
		else
			max_pdu_len = LDP_MAX_LEN;
		if (pdu_len < (LDP_HDR_PDU_LEN + LDP_MSG_SIZE) ||
		    pdu_len > max_pdu_len) {
			session_shutdown(nbr, S_BAD_PDU_LEN, 0, 0);
			free(buf);
			return (0);
		}
		pdu_len -= LDP_HDR_PDU_LEN;
		if (ldp_hdr->lsr_id != nbr->id.s_addr ||
		    ldp_hdr->lspace_id != 0) {
			session_shutdown(nbr, S_BAD_LDP_ID, 0, 0);
			free(buf);
			return (0);
		}
		pdu += LDP_HDR_SIZE;
		len -= LDP_HDR_SIZE;

		nbr_fsm(nbr, NBR_EVT_PDU_RCVD);

		while (len >= LDP_MSG_SIZE) {
			uint16_t type;

			msg = (struct ldp_msg *)pdu;
			type = ntohs(msg->type);
			msg_len = ntohs(msg->length);
			msg_size = msg_len + LDP_MSG_DEAD_LEN;
			if (msg_len < LDP_MSG_LEN || msg_size > pdu_len) {
				session_shutdown(nbr, S_BAD_TLV_LEN, msg->id,
				    msg->type);
				free(buf);
				return (0);
			}
			pdu_len -= msg_size;

			/* check for error conditions earlier */
			switch (type) {
			case MSG_TYPE_INIT:
				if ((nbr->state != NBR_STA_INITIAL) &&
				    (nbr->state != NBR_STA_OPENSENT)) {
					session_shutdown(nbr, S_SHUTDOWN,
					    msg->id, msg->type);
					free(buf);
					return (0);
				}
				break;
			case MSG_TYPE_KEEPALIVE:
				if ((nbr->state == NBR_STA_INITIAL) ||
				    (nbr->state == NBR_STA_OPENSENT)) {
					session_shutdown(nbr, S_SHUTDOWN,
					    msg->id, msg->type);
					free(buf);
					return (0);
				}
				break;
			case MSG_TYPE_ADDR:
			case MSG_TYPE_ADDRWITHDRAW:
			case MSG_TYPE_LABELMAPPING:
			case MSG_TYPE_LABELREQUEST:
			case MSG_TYPE_LABELWITHDRAW:
			case MSG_TYPE_LABELRELEASE:
			case MSG_TYPE_LABELABORTREQ:
				if (nbr->state != NBR_STA_OPER) {
					session_shutdown(nbr, S_SHUTDOWN,
					    msg->id, msg->type);
					free(buf);
					return (0);
				}
				break;
			default:
				break;
			}

			/* switch LDP packet type */
			switch (type) {
			case MSG_TYPE_NOTIFICATION:
				ret = recv_notification(nbr, pdu, msg_size);
				break;
			case MSG_TYPE_INIT:
				ret = recv_init(nbr, pdu, msg_size);
				break;
			case MSG_TYPE_KEEPALIVE:
				ret = recv_keepalive(nbr, pdu, msg_size);
				break;
			case MSG_TYPE_ADDR:
			case MSG_TYPE_ADDRWITHDRAW:
				ret = recv_address(nbr, pdu, msg_size);
				break;
			case MSG_TYPE_LABELMAPPING:
			case MSG_TYPE_LABELREQUEST:
			case MSG_TYPE_LABELWITHDRAW:
			case MSG_TYPE_LABELRELEASE:
			case MSG_TYPE_LABELABORTREQ:
				ret = recv_labelmessage(nbr, pdu, msg_size,
				    type);
				break;
			default:
				log_debug("%s: unknown LDP message from nbr %s",
				    __func__, inet_ntoa(nbr->id));
				if (!(ntohs(msg->type) & UNKNOWN_FLAG))
					send_notification_nbr(nbr,
					    S_UNKNOWN_MSG, msg->id, msg->type);
				/* ignore the message */
				ret = 0;
				break;
			}

			if (ret == -1) {
				/* parser failed, giving up */
				free(buf);
				return (0);
			}

			/* Analyse the next message */
			pdu += msg_size;
			len -= msg_size;
		}
		free(buf);
		if (len != 0) {
			session_shutdown(nbr, S_BAD_PDU_LEN, 0, 0);
			return (0);
		}
	}

	return (0);
}

static int
session_write(struct thread *thread)
{
	struct tcp_conn *tcp = THREAD_ARG(thread);
	struct nbr	*nbr = tcp->nbr;

	tcp->wbuf.ev = NULL;

	if (msgbuf_write(&tcp->wbuf.wbuf) <= 0)
		if (errno != EAGAIN && nbr)
			nbr_fsm(nbr, NBR_EVT_CLOSE_SESSION);

	if (nbr == NULL && !tcp->wbuf.wbuf.queued) {
		/*
		 * We are done sending the notification message, now we can
		 * close the socket.
		 */
		tcp_close(tcp);
		return (0);
	}

	evbuf_event_add(&tcp->wbuf);

	return (0);
}

void
session_shutdown(struct nbr *nbr, uint32_t status, uint32_t msg_id,
    uint32_t msg_type)
{
	switch (nbr->state) {
	case NBR_STA_PRESENT:
		if (nbr_pending_connect(nbr))
			THREAD_WRITE_OFF(nbr->ev_connect);
		break;
	case NBR_STA_INITIAL:
	case NBR_STA_OPENREC:
	case NBR_STA_OPENSENT:
	case NBR_STA_OPER:
		log_debug("%s: lsr-id %s", __func__, inet_ntoa(nbr->id));

		send_notification_nbr(nbr, status, msg_id, msg_type);

		nbr_fsm(nbr, NBR_EVT_CLOSE_SESSION);
		break;
	default:
		fatalx("session_shutdown: unknown neighbor state");
	}
}

void
session_close(struct nbr *nbr)
{
	log_debug("%s: closing session with lsr-id %s", __func__,
	    inet_ntoa(nbr->id));

	tcp_close(nbr->tcp);
	nbr_stop_ktimer(nbr);
	nbr_stop_ktimeout(nbr);
	nbr_stop_itimeout(nbr);
}

static ssize_t
session_get_pdu(struct ibuf_read *r, char **b)
{
	struct ldp_hdr	l;
	size_t		av, dlen, left;

	av = r->wpos;
	if (av < sizeof(l))
		return (0);

	memcpy(&l, r->buf, sizeof(l));
	dlen = ntohs(l.length) + LDP_HDR_DEAD_LEN;
	if (dlen > av)
		return (0);

	if ((*b = malloc(dlen)) == NULL)
		return (-1);

	memcpy(*b, r->buf, dlen);
	if (dlen < av) {
		left = av - dlen;
		memmove(r->buf, r->buf + dlen, left);
		r->wpos = left;
	} else
		r->wpos = 0;

	return (dlen);
}

struct tcp_conn *
tcp_new(int fd, struct nbr *nbr)
{
	struct tcp_conn		*tcp;
	struct sockaddr_storage	 src;
	socklen_t		 len = sizeof(src);

	if ((tcp = calloc(1, sizeof(*tcp))) == NULL)
		fatal(__func__);

	tcp->fd = fd;
	evbuf_init(&tcp->wbuf, tcp->fd, session_write, tcp);

	if (nbr) {
		if ((tcp->rbuf = calloc(1, sizeof(struct ibuf_read))) == NULL)
			fatal(__func__);

		tcp->rev = thread_add_read(master, session_read, nbr, tcp->fd);
		tcp->nbr = nbr;
	}

	getsockname(fd, (struct sockaddr *)&src, &len);
	sa2addr((struct sockaddr *)&src, NULL, NULL, &tcp->lport);
	getpeername(fd, (struct sockaddr *)&src, &len);
	sa2addr((struct sockaddr *)&src, NULL, NULL, &tcp->rport);

	return (tcp);
}

static void
tcp_close(struct tcp_conn *tcp)
{
	/* try to flush write buffer */
	msgbuf_write(&tcp->wbuf.wbuf);
	evbuf_clear(&tcp->wbuf);

	if (tcp->nbr) {
		THREAD_READ_OFF(tcp->rev);
		free(tcp->rbuf);
		tcp->nbr->tcp = NULL;
	}

	close(tcp->fd);
	accept_unpause();
	free(tcp);
}

static struct pending_conn *
pending_conn_new(int fd, int af, union ldpd_addr *addr)
{
	struct pending_conn	*pconn;

	if ((pconn = calloc(1, sizeof(*pconn))) == NULL)
		fatal(__func__);

	pconn->fd = fd;
	pconn->af = af;
	pconn->addr = *addr;
	TAILQ_INSERT_TAIL(&global.pending_conns, pconn, entry);
	pconn->ev_timeout = thread_add_timer(master, pending_conn_timeout,
	    pconn, PENDING_CONN_TIMEOUT);

	return (pconn);
}

void
pending_conn_del(struct pending_conn *pconn)
{
	THREAD_TIMER_OFF(pconn->ev_timeout);
	TAILQ_REMOVE(&global.pending_conns, pconn, entry);
	free(pconn);
}

struct pending_conn *
pending_conn_find(int af, union ldpd_addr *addr)
{
	struct pending_conn	*pconn;

	TAILQ_FOREACH(pconn, &global.pending_conns, entry)
		if (af == pconn->af &&
		    ldp_addrcmp(af, addr, &pconn->addr) == 0)
			return (pconn);

	return (NULL);
}

static int
pending_conn_timeout(struct thread *thread)
{
	struct pending_conn	*pconn = THREAD_ARG(thread);
	struct tcp_conn		*tcp;

	pconn->ev_timeout = NULL;

	log_debug("%s: no adjacency with remote end: %s", __func__,
	    log_addr(pconn->af, &pconn->addr));

	/*
	 * Create a write buffer detached from any neighbor to send a
	 * notification message reliably.
	 */
	tcp = tcp_new(pconn->fd, NULL);
	send_notification(S_NO_HELLO, tcp, 0, 0);
	msgbuf_write(&tcp->wbuf.wbuf);

	pending_conn_del(pconn);

	return (0);
}
