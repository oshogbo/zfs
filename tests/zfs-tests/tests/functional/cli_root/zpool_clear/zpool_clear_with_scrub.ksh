#!/bin/ksh -p
# SPDX-License-Identifier: CDDL-1.0
#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or https://opensource.org/licenses/CDDL-1.0.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#

#
# Copyright (c) 2026 Klara Inc.
#
# This software was developed by Mariusz Zaborski <oshogbo@FreeBSD.org>
# under sponsorship from Wasabi Technology, Inc. and Klara Inc.
#

. $STF_SUITE/include/libtest.shlib
. $STF_SUITE/include/kstat.shlib

#
# DESCRIPTION:
#	Verify that clearing a suspended pool automatically triggers
#	a scrub of recent data.
#
# STRATEGY:
#	 1. Create a mirrored pool with failmode=wait.
#	 2. Write some data to the pool.
#	 3. Inject I/O errors on all devices to suspend the pool.
#	 4. Wait for the pool to become suspended.
#	 5. Clear the injected errors.
#	 6. Run `zpool clear` to resume the pool.
#	 7. Verify that a scrub was automatically started.
#

verify_runnable "global"

function cleanup
{
	zinject -c all
	if poolexists $TESTPOOL; then
		zpool clear $TESTPOOL
		destroy_pool $TESTPOOL
	fi
}

log_onexit cleanup

log_assert "Clearing a suspended pool automatically triggers a recent scrub."

read -r DISK1 DISK2 _ <<<"$DISKS"

log_must zpool create -o failmode=wait -f $TESTPOOL mirror $DISK1 $DISK2

log_must dd if=/dev/urandom of=/$TESTPOOL/testfile bs=128k count=10
log_must zpool sync $TESTPOOL

# Inject write errors on both devices to cause pool suspension.
log_must zinject -d $DISK1 -e io -T write $TESTPOOL
log_must zinject -d $DISK2 -e io -T write $TESTPOOL

# Trigger writes to suspend the pool.
dd if=/dev/urandom of=/$TESTPOOL/triggerfile bs=128k count=1 2>/dev/null &
typeset -i pid=$!

log_note "Waiting for pool to suspend"
typeset -i tries=10
until [[ $(kstat_pool $TESTPOOL state) == "SUSPENDED" ]] ; do
	if ((tries-- == 0)); then
		log_fail "pool didn't suspend"
	fi
	sleep 1
done

# Remove injected errors and clear the pool.
log_must zinject -c all

log_must zpool clear $TESTPOOL

# Wait for the background write to finish.
wait $pid

# Verify that a scrub was automatically started after clearing
# the suspended pool.
log_must eval "zpool status $TESTPOOL | grep -q 'scan:.*scrub'"

log_pass "Clearing a suspended pool automatically triggered a recent scrub."
