#!/bin/ksh -p
# SPDX-License-Identifier: CDDL-1.0

#
# This file and its contents are supplied under the terms of the
# Common Development and Distribution License ("CDDL"), version 1.0.
# You may only use this file in accordance with the terms of version
# 1.0 of the CDDL.
#
# A full copy of the text of the CDDL should have accompanied this
# source.  A copy of the CDDL is also available via the Internet at
# http://www.illumos.org/license/CDDL.
#

#
# Copyright (c) 2026, Klara Inc.
#
# This software was developed by
# Mariusz Zaborski <mariusz.zaborski@klarasystems.com>
# under sponsorship from Wasabi Technology, Inc. and Klara Inc.
#

. $STF_SUITE/include/libtest.shlib

#
# Description:
# zdb -c --scrub-min-txg/--scrub-max-txg skips checksum verification
# for blocks born inside the given txg range and still verifies
# blocks born outside of it.
#
# Strategy:
# 1. Create a pool and write file1, recording the txg range it was
#    born in.  Export and import so file2 lands in a disjoint range.
# 2. Write file2, recording its txg range.
# 3. Corrupt both files on disk and verify:
#    - a full zdb -cc detects the corruption
#    - a skip range covering both files passes
#    - a skip range covering only one file fails on the other
#

function cleanup
{
	datasetexists $TESTPOOL && destroy_pool $TESTPOOL
}

#
# Return the txg of the current uberblock.  Must not be called from a
# command substitution together with functions that log to stdout.
#
function pool_txg
{
	zdb -u $TESTPOOL | awk '$1 == "txg" {print $3; exit}'
}

log_assert "zdb -c --scrub-min-txg/--scrub-max-txg limits verification" \
    "to blocks born outside the given txg range."
log_onexit cleanup

verify_runnable "global"

set -A disks $DISKS
default_setup_noexit ${disks[0]}

sync_pool $TESTPOOL
txg_base=$(pool_txg)

file_write -o create -w -f $TESTDIR/file1 -b 131072 -c 8
sync_pool $TESTPOOL
txg_file1=$(pool_txg)

log_must zpool export $TESTPOOL
log_must zpool import $TESTPOOL

file_write -o create -w -f $TESTDIR/file2 -b 131072 -c 8
sync_pool $TESTPOOL
txg_file2=$(pool_txg)

((txg_base < txg_file1 && txg_file1 < txg_file2)) ||
	log_fail "txgs did not advance: $txg_base $txg_file1 $txg_file2"

# file1 was born in (txg_base, txg_file1], file2 in (txg_file1, txg_file2].
# The skip range bounds are exclusive, so widen the upper bounds by one.
corrupt_blocks_at_level $TESTDIR/file1
corrupt_blocks_at_level $TESTDIR/file2

# A full verification must detect the corruption.
log_mustnot zdb -cc $TESTPOOL

# Skipping both files must hide it.  The hex bound exercises the
# base-0 txg parsing shared with -t/--txg.
log_must zdb -cc --scrub-min-txg=$txg_base \
    --scrub-max-txg=$(printf '0x%x' $((txg_file2 + 1))) $TESTPOOL

# Skipping only one file must still fail on the other.
log_mustnot zdb -cc --scrub-min-txg=$txg_base \
    --scrub-max-txg=$((txg_file1 + 1)) $TESTPOOL
log_mustnot zdb -cc --scrub-min-txg=$txg_file1 \
    --scrub-max-txg=$((txg_file2 + 1)) $TESTPOOL

log_pass "zdb -c --scrub-min-txg/--scrub-max-txg limits verification" \
    "to blocks born outside the given txg range."
