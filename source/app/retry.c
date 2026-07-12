#include "retry.h"

cb_retry_status cb_retry(int max_attempts,
                         cb_retry_predicate retryable,
                         cb_retry_status (*op)(void *ctx),
                         void *ctx,
                         unsigned (*backoff_ms)(int attempt),
                         void (*sleep_ms)(unsigned ms)) {
    cb_retry_status status;
    int attempt;

    if (max_attempts <= 0) max_attempts = 1;
    if (!op) return 0;

    for (attempt = 0; attempt < max_attempts; attempt++) {
        status = op(ctx);
        if (!retryable || !retryable(status)) return status;
        if (attempt + 1 >= max_attempts) return status;
        if (backoff_ms) {
            unsigned delay = backoff_ms(attempt);
            if (sleep_ms && delay) sleep_ms(delay);
        }
    }
    return status;
}
