#ifndef __CR_LUO_H__
#define __CR_LUO_H__

#include <stdbool.h>
#include <stdint.h>

/*
 * Glue for the Linux liveupdate (LUO) uAPI (/dev/liveupdate).
 *
 * Used by the --luo-session option to preserve memfd contents across a
 * kexec instead of serializing their pages into images.
 *
 * All functions return 0 (or a new fd) on success and a negative errno
 * on failure, so callers can map ENOTTY/EOPNOTSUPP/ENOENT onto fallback
 * decisions (the LUO uAPI is not stable yet; see
 * Documentation/core-api/liveupdate.rst).
 */

/* True if opts.luo_session was given. */
bool luo_mode_enabled(void);

/*
 * Create a new session on /dev/liveupdate (dump side).
 * Returns the session fd, or a negative errno.
 */
int luo_session_create(const char *name);

/*
 * Retrieve a preserved session by name after a live update
 * (restore side; system must be in LIVEUPDATE_STATE_UPDATED).
 * Returns the session fd, or a negative errno (-ENOENT if unknown).
 */
int luo_session_retrieve(const char *name);

/*
 * Preserve fd in the session under the caller-chosen token.
 * Returns 0 or a negative errno.
 */
int luo_preserve_fd(int session_fd, int fd, uint64_t token);

/*
 * Retrieve a preserved fd by token. Returns the new fd, or a negative
 * errno. A failed retrieve permanently fails for that token.
 */
int luo_retrieve_fd(int session_fd, uint64_t token);

/*
 * Release kernel ownership of a retrieved session after all its fds
 * were restored. Restore side only; retriable on failure.
 * Returns 0 or a negative errno.
 */
int luo_session_finish(int session_fd);

#endif /* __CR_LUO_H__ */
