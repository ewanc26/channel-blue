#include "app/retry.h"

#include <assert.h>
#include <stdio.h>

/* ---- instrumentation shared by the tests ---- */

static int g_calls;
static int g_backoff_attempts[16];
static unsigned g_backoff_values[16];
static int g_sleep_calls;
static unsigned g_sleep_values[16];

static cb_retry_status scripted_op(void *ctx) {
    const int *script = ctx;
    int idx = g_calls;
    g_calls++;
    return script[idx];
}

static int always_transient(cb_retry_status status) {
    (void)status;
    return 1;
}

static int never_transient(cb_retry_status status) {
    (void)status;
    return 0;
}

static int transient_when_nonzero(cb_retry_status status) {
    return status != 0;
}

static unsigned recorded_backoff(int attempt) {
    g_backoff_attempts[g_sleep_calls] = attempt;
    g_backoff_values[g_sleep_calls] = (unsigned)(500u << attempt);
    return g_backoff_values[g_sleep_calls];
}

static void recorded_sleep(unsigned ms) {
    g_sleep_values[g_sleep_calls] = ms;
    g_sleep_calls++;
}

static void reset(void) {
    g_calls = 0;
    g_sleep_calls = 0;
    for (int i = 0; i < 16; i++) {
        g_backoff_attempts[i] = -1;
        g_backoff_values[i] = 0;
        g_sleep_values[i] = 0;
    }
}

int main(void) {
    int script[8];

    /* success on the first attempt: called once, no backoff */
    reset();
    script[0] = 0;
    assert(cb_retry(4, transient_when_nonzero, scripted_op, script,
                    recorded_backoff, recorded_sleep) == 0);
    assert(g_calls == 1);
    assert(g_sleep_calls == 0);

    /* transient every time: called max_attempts times, last status returned */
    reset();
    script[0] = 3; script[1] = 3; script[2] = 3; script[3] = 3;
    assert(cb_retry(4, always_transient, scripted_op, script,
                    recorded_backoff, recorded_sleep) == 3);
    assert(g_calls == 4);
    assert(g_sleep_calls == 3);
    /* backoff used attempt indices 0,1,2 and exponential 500/1000/2000 */
    assert(g_backoff_attempts[0] == 0 && g_sleep_values[0] == 500);
    assert(g_backoff_attempts[1] == 1 && g_sleep_values[1] == 1000);
    assert(g_backoff_attempts[2] == 2 && g_sleep_values[2] == 2000);

    /* transient twice then success: stops early, no further backoff */
    reset();
    script[0] = 7; script[1] = 7; script[2] = 0;
    assert(cb_retry(5, transient_when_nonzero, scripted_op, script,
                    recorded_backoff, recorded_sleep) == 0);
    assert(g_calls == 3);
    assert(g_sleep_calls == 2);

    /* non-retryable status: single attempt, no backoff */
    reset();
    script[0] = 15;
    assert(cb_retry(4, never_transient, scripted_op, script,
                    recorded_backoff, recorded_sleep) == 15);
    assert(g_calls == 1);
    assert(g_sleep_calls == 0);

    /* max_attempts <= 0 is treated as 1 */
    reset();
    script[0] = 3;
    assert(cb_retry(0, always_transient, scripted_op, script,
                    recorded_backoff, recorded_sleep) == 3);
    assert(g_calls == 1);

    /* NULL sleep_ms: backoff computed but no sleep performed */
    reset();
    script[0] = 3; script[1] = 3; script[2] = 3; script[3] = 3;
    assert(cb_retry(4, always_transient, scripted_op, script,
                    recorded_backoff, NULL) == 3);
    assert(g_calls == 4);
    assert(g_sleep_calls == 0);

    puts("retry tests passed");
    return 0;
}
