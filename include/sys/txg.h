/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or https://opensource.org/licenses/CDDL-1.0.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2010 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */
/*
 * Copyright (c) 2012, 2017 by Delphix. All rights reserved.
 */

#ifndef _SYS_TXG_H
#define	_SYS_TXG_H

#include <sys/spa.h>
#include <sys/zfs_context.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	TXG_CONCURRENT_STATES	3	/* open, quiescing, syncing	*/
#define	TXG_SIZE		4		/* next power of 2	*/
#define	TXG_MASK		(TXG_SIZE - 1)	/* mask for size	*/
#define	TXG_INITIAL		TXG_SIZE	/* initial txg 		*/
#define	TXG_IDX			(txg & TXG_MASK)
#define	TXG_UNKNOWN		0

/* Number of txgs worth of frees we defer adding to in-core spacemaps */
#define	TXG_DEFER_SIZE		2

typedef struct tx_cpu tx_cpu_t;

typedef struct txg_handle {
	tx_cpu_t	*th_cpu;
	uint64_t	th_txg;
} txg_handle_t;

typedef struct txg_node {
	struct txg_node	*tn_next[TXG_SIZE];
	uint8_t		tn_member[TXG_SIZE];
} txg_node_t;

typedef struct txg_list {
	kmutex_t	tl_lock;
	size_t		tl_offset;
	spa_t		*tl_spa;
	txg_node_t	*tl_head[TXG_SIZE];
} txg_list_t;

struct dsl_pool;
struct dmu_tx;

/*
 * TXG wait flags, used by txg_wait_synced_tx and callers to indicate
 * modifications to how they wish to wait for a txg.
 */
typedef enum {
	/* No special wait flags. */
	TXG_WAIT_F_NONE		= 0,
	/* Reject the call with EINTR upon receiving a signal. */
	TXG_WAIT_F_SIGNAL	= (1U << 0),
	/* Reject the call with EAGAIN upon suspension. */
	TXG_WAIT_F_NOSUSPEND	= (1U << 1),
	/* Ignore errors and export anyway. */
	TXG_WAIT_F_FORCE_EXPORT	= (1U << 2),
} txg_wait_flag_t;

extern void txg_init(struct dsl_pool *dp, uint64_t txg);
extern void txg_fini(struct dsl_pool *dp);
extern void txg_sync_start(struct dsl_pool *dp);
extern int txg_sync_stop(struct dsl_pool *dp, txg_wait_flag_t txg_how);
extern uint64_t txg_hold_open(struct dsl_pool *dp, txg_handle_t *txghp);
extern void txg_rele_to_quiesce(txg_handle_t *txghp);
extern void txg_rele_to_sync(txg_handle_t *txghp);
extern void txg_register_callbacks(txg_handle_t *txghp, list_t *tx_callbacks);

extern void txg_delay(struct dsl_pool *dp, uint64_t txg, hrtime_t delta,
    hrtime_t resolution);
extern void txg_kick(struct dsl_pool *dp, uint64_t txg);

/*
 * Wait until the given transaction group has finished syncing.
 * Try to make this happen as soon as possible (eg. kick off any
 * necessary syncs immediately).  If txg==0, wait for the currently open
 * txg to finish syncing.  This may be interrupted due to an exiting pool.
 *
 * If desired, flags can be specified using txg_wait_synced_tx(), in case
 * the caller wants to be interruptible.
 */
extern void txg_wait_synced(struct dsl_pool *dp, uint64_t txg);
extern int txg_wait_synced_tx(struct dsl_pool *dp, uint64_t txg,
    struct dmu_tx *tx, txg_wait_flag_t flags);
extern int txg_wait_synced_flags(struct dsl_pool *dp, uint64_t txg,
    txg_wait_flag_t flags);

/*
 * Similar to a txg_wait_synced but it can be interrupted from a signal.
 * Returns B_TRUE if the thread was signaled while waiting.
 */
#define	txg_wait_synced_sig(dp, txg)				\
	(txg_wait_synced_tx(dp, txg, NULL, TXG_WAIT_F_SIGNAL) == EINTR)

/*
 * Wait until the given transaction group, or one after it, is
 * the open transaction group.  Try to make this happen as soon
 * as possible (eg. kick off any necessary syncs immediately) when
 * should_quiesce is set.  If txg == 0, wait for the next open txg.
 */
extern void txg_wait_open(struct dsl_pool *dp, uint64_t txg,
    boolean_t should_quiesce);

void txg_force_export(spa_t *spa);

/*
 * Returns TRUE if we are "backed up" waiting for the syncing
 * transaction to complete; otherwise returns FALSE.
 */
extern boolean_t txg_stalled(struct dsl_pool *dp);

/* returns TRUE if someone is waiting for the next txg to sync */
extern boolean_t txg_sync_waiting(struct dsl_pool *dp);

extern void txg_verify(spa_t *spa, uint64_t txg);

extern void txg_completion_notify(struct dsl_pool *dp);

/*
 * Wait for pending commit callbacks of already-synced transactions to finish
 * processing.
 */
extern void txg_wait_callbacks(struct dsl_pool *dp);

/*
 * Per-txg object lists.
 */

#define	TXG_CLEAN(txg)	((txg) - 1)

extern void txg_list_create(txg_list_t *tl, spa_t *spa, size_t offset);
extern void txg_list_destroy(txg_list_t *tl);
extern boolean_t txg_list_empty(txg_list_t *tl, uint64_t txg);
extern boolean_t txg_all_lists_empty(txg_list_t *tl);
extern boolean_t txg_list_add(txg_list_t *tl, void *p, uint64_t txg);
extern boolean_t txg_list_add_tail(txg_list_t *tl, void *p, uint64_t txg);
extern void *txg_list_remove(txg_list_t *tl, uint64_t txg);
extern void *txg_list_remove_this(txg_list_t *tl, void *p, uint64_t txg);
extern boolean_t txg_list_member(txg_list_t *tl, void *p, uint64_t txg);
extern void *txg_list_head(txg_list_t *tl, uint64_t txg);
extern void *txg_list_next(txg_list_t *tl, void *p, uint64_t txg);

/* Global tuning */
extern uint_t zfs_txg_timeout;


#ifdef ZFS_DEBUG
#define	TXG_VERIFY(spa, txg)		txg_verify(spa, txg)
#else
#define	TXG_VERIFY(spa, txg)
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_TXG_H */
