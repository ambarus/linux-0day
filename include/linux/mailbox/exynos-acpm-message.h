/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _LINUX_EXYNOS_ACPM_MESSAGE_H_
#define _LINUX_EXYNOS_ACPM_MESSAGE_H_

#include <linux/types.h>

/**
 * struct exynos_acpm_message - Exynos ACPM mailbox message format
 * @cmd: pointer to u32 command.
 *
 */
struct exynos_acpm_message {
	size_t len;
        u32 *cmd;
};

#endif /* _LINUX_EXYNOS_ACPM_MESSAGE_H_ */
