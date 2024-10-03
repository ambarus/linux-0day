/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef __MAILBOX_REQUEST_H
#define __MAILBOX_REQUEST_H

#include <linux/types.h>
#include <linux/completion.h>

struct mbox_wait {
	struct completion completion;
	int err;
};

#define DECLARE_MBOX_WAIT(_wait) \
	struct mbox_wait _wait = { \
		COMPLETION_INITIALIZER_ONSTACK((_wait).completion), 0 }

struct mbox_request {
	void *tx;
	void *rx;
	struct mbox_wait *wait;
};

#endif /* __MAILBOX_H */
