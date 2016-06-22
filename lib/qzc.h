#ifndef _QZC_H
#define _QZC_H

#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>

#include "qerr.h"
#include "qzc.capnp-gen.h"

struct qzc_nodetype {
	uint64_t tid;
	ptrdiff_t node_member_offset;
	void (*get)(void *entity, struct QZCGetReq *req, struct QZCGetRep *rep,
			struct QZCReply *rep_err, struct capn_segment *seg);
	void (*createchild)(void *parent, struct QZCCreateReq *req,
			struct QZCCreateRep *rep,
			struct QZCReply *rep_err, struct capn_segment *seg);
	void (*set)(void *entity, struct QZCSetReq *req,
			struct QZCReply *rep_err, struct capn_segment *seg);
	void (*unset)(void *entity, struct QZCSetReq *req,
			struct QZCReply *rep_err, struct capn_segment *seg);
	void (*destroy)(void *entity, struct QZCDelReq *req,
			struct QZCReply *rep_err, struct capn_segment *seg);
};

struct qzc_node {
	uint64_t nid;
	struct qzc_nodetype *type;
};

#define QZC_NODE \
	struct qzc_node qzc_node;

#define QZC_NODE_REG(n, structname) \
	qzc_node_reg(&n->qzc_node, &qzc_t_ ## structname);
#define QZC_NODE_UNREG(n) \
	qzc_node_unreg(&n->qzc_node);

void qzc_node_reg(struct qzc_node *node, struct qzc_nodetype *type);
void qzc_node_unreg(struct qzc_node *node);

#define EXT_QZC_NODETYPE(structname) \
	extern struct qzc_nodetype qzc_t_ ## structname;
#define QZC_NODETYPE(structname, id) \
	struct qzc_nodetype qzc_t_ ## structname = { \
		.tid = id, \
		.node_member_offset = \
			(ptrdiff_t)offsetof(struct structname, qzc_node) \
	};
void qzc_nodetype_init(struct qzc_nodetype *type);

struct qzc_sock;

void qzc_init(void);
void qzc_finish(void);
struct qzc_sock *qzc_bind(struct thread_master *master, const char *url);
void qzc_close(struct qzc_sock *sock);

struct qzc_wkn {
	uint64_t wid;
	uint64_t (*resolve)(void);

	struct qzc_wkn *next;
};
void qzc_wkn_reg(struct qzc_wkn *wkn);

void qzc_err_set_loc(struct QZCReply *rep, struct capn_segment *cs,
		enum qerr_kind category, uint32_t unique_id,
		const char *message,
		const char *file, int line, const char *func);
#define qzc_err_set(rep, cs, cat, uid, msg) \
	qzc_err_set_loc(rep, cs, cat, uid, msg, __FILE__, __LINE__, __func__)
void qzc_err_node(struct QZCReply *rep, struct capn_segment *cs, uint64_t nid);
void qzc_err_elem(struct QZCReply *rep, struct capn_segment *cs,
		uint64_t nid, uint64_t elem);
void qzc_err_data(struct QZCReply *rep, struct capn_segment *cs,
		uint64_t nid, uint64_t elem,
		const uint32_t *ordinals, size_t len);

struct qcaperr {
	struct QZCReply *rep;
	struct capn_segment *seg;
	uint64_t nid;
	uint64_t elem;
	bool set;
};

#endif /* _QZC_H */
