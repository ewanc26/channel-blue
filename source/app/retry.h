#ifndef CB_RETRY_H
#define CB_RETRY_H

#include <stddef.h>

/* Status values are platform-specific (e.g. wf_status); retry treats them as
 * plain ints so the primitive stays decoupled from any single transport. */
typedef int cb_retry_status;

/* Return non-zero if `status` is transient and worth another attempt. */
typedef int (*cb_retry_predicate)(cb_retry_status status);

/*
 * Run `op(ctx)` up to `max_attempts` times, stopping at the first status for
 * which `retryable` returns 0 (or once `max_attempts` is exhausted). Before
 * each retry it sleeps `backoff_ms(attempt)` milliseconds through `sleep_ms`;
 * pass NULL for `backoff_ms` to wait 0 ms, or NULL for `sleep_ms` to skip
 * sleeping entirely. `attempt` is the 0-based index of the attempt that just
 * failed. Returns the last status observed. A `max_attempts` of <= 0 is
 * treated as 1. `op` must not be NULL.
 */
cb_retry_status cb_retry(int max_attempts,
                         cb_retry_predicate retryable,
                         cb_retry_status (*op)(void *ctx),
                         void *ctx,
                         unsigned (*backoff_ms)(int attempt),
                         void (*sleep_ms)(unsigned ms));

#endif /* CB_RETRY_H */
