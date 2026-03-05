#ifndef SENTRY_CLIENT_H
#define SENTRY_CLIENT_H

#include <stddef.h>
#include <stdbool.h>

typedef struct {
    char  *data;
    size_t size;
} Memory;

/**
 * load_env - parse a .env file and inject key=value pairs into the
 *            process environment via setenv(3).
 *
 * @filename: path to the .env file (e.g. ".env")
 *
 * Lines beginning with '#' and empty lines are ignored.
 * Existing environment variables are overwritten.
 */
void load_env(const char *filename);

/**
 * build_sentry_event_list_url - build the Sentry REST URL that returns
 *   the list of events for an issue.
 *
 *   https://sentry.io/api/0/organizations/{org}/issues/{issue}/events/
 *
 * @org:      Sentry organisation slug
 * @issue:    Sentry issue ID (numeric string)
 *
 * Returns a heap-allocated, NUL-terminated URL string.
 * The caller is responsible for free()ing it.
 * Returns NULL on allocation failure.
 */
char *build_sentry_event_list_url(const char *org, const char *issue);

/**
 * build_sentry_event_detail_url - build the Sentry REST URL for a
 *   single event's full detail payload.
 *
 *   https://sentry.io/api/0/organizations/{org}/issues/{issue}/events/{event_id}/
 *
 * @org:      Sentry organisation slug
 * @issue:    Sentry issue ID
 * @event_id: individual event ID (UUID / hex string)
 *
 * Returns a heap-allocated, NUL-terminated URL string.
 * The caller is responsible for free()ing it.
 * Returns NULL on allocation failure.
 */
char *build_sentry_event_detail_url(const char *org,
                                    const char *issue,
                                    const char *event_id);

/**
 * http_get_with_token - perform an HTTP GET request, sending a Bearer
 *   token in the Authorization header.
 *
 * @url:   fully-qualified URL to fetch
 * @token: Sentry auth token (without the "Bearer " prefix)
 *
 * Returns a heap-allocated, NUL-terminated response body.
 * The caller is responsible for free()ing it.
 * Returns NULL on curl initialisation failure or network error.
 */
char *http_get_with_token(const char *url, const char *token);

/**
 * process_event_list - iterate a JSON array of Sentry events.
 *
 * Prints each eventID to stdout.  When @detail is true, fetches the
 * full detail JSON for every event from the Sentry API and concatenates
 * all payloads (newline-separated) into the returned string.
 *
 * @json:     JSON array string returned by the event-list endpoint
 * @org:      Sentry organisation slug
 * @issue_id: Sentry issue ID
 * @token:    Sentry auth token
 * @detail:   when true, fetch per-event detail from the API
 *
 * Returns a heap-allocated, NUL-terminated string containing all
 * concatenated event detail JSON bodies (empty string when !detail).
 * The caller is responsible for free()ing it.
 * Returns NULL on JSON parse failure or allocation failure.
 */
char *process_event_list(const char *json,
                         const char *org,
                         const char *issue_id,
                         const char *token,
                         bool        detail);

/**
 * build_analysis_prompt - compose a Claude prompt requesting a full
 *   incident-analysis document suite for a Sentry issue.
 *
 * @issue_id:              Sentry issue ID (used in folder path & prompt)
 * @sentry_issue_response: raw JSON from the issue endpoint (may be NULL)
 * @all_events:            concatenated event detail JSON (may be NULL)
 *
 * Returns a heap-allocated, NUL-terminated prompt string.
 * The caller is responsible for free()ing it.
 * Returns NULL on allocation failure.
 */
char *build_analysis_prompt(const char *issue_id,
                            const char *sentry_issue_response,
                            const char *all_events);

/**
 * build_duplicate_prompt - compose a Claude prompt requesting duplicate-
 *   issue documentation for a pair of Sentry issues.
 *
 * @issue_id:              the current (duplicate) Sentry issue ID
 * @duplicate_issue_id:    the primary issue this duplicates
 * @sentry_issue_response: raw JSON from the issue endpoint (may be NULL)
 * @all_events:            concatenated event detail JSON (may be NULL)
 *
 * Returns a heap-allocated, NUL-terminated prompt string.
 * The caller is responsible for free()ing it.
 * Returns NULL on allocation failure.
 */
char *build_duplicate_prompt(const char *issue_id,
                             const char *duplicate_issue_id,
                             const char *sentry_issue_response,
                             const char *all_events);

/**
 * run_claude_prompt - pipe @prompt into the `claude` CLI and capture
 *   its stdout as a heap-allocated string.
 *
 * @prompt: NUL-terminated prompt text
 *
 * Returns a heap-allocated, NUL-terminated response string.
 * The caller is responsible for free()ing it.
 * Returns NULL on popen() failure or allocation failure.
 *
 * NOTE: the prompt is passed via a temporary file to avoid shell-quoting
 * issues with arbitrary JSON content inside the prompt string.
 */
char *run_claude_prompt(const char *prompt);

#endif /* SENTRY_CLIENT_H */
