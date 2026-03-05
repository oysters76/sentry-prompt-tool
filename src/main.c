#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <getopt.h>

#include "sentry_client.h"

int main(int argc, char **argv)
{
    bool detail    = false;
    bool claude    = false;
    bool duplicate = false;

    static struct option long_options[] = {
        {"detail",    no_argument, 0, 'd'},
        {"claude",    no_argument, 0, 'c'},
        {"duplicate", no_argument, 0, 'u'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "dcu", long_options, NULL)) != -1) {
        switch (opt) {
        case 'd': detail    = true; break;
        case 'c': claude    = true; break;
        case 'u': duplicate = true; break;
        default:
            printf("Usage: sentry-tool [-d|--detail] [-c|--claude] "
                   "[-u|--duplicate] ISSUE_ID [DUPLICATE_ISSUE_ID]\n");
            exit(EXIT_FAILURE);
        }
    }

    if (optind >= argc) {
        printf("Error: missing ISSUE_ID\n");
        printf("Usage: sentry-tool [-d|--detail] [-c|--claude] "
               "[-u|--duplicate] ISSUE_ID [DUPLICATE_ISSUE_ID]\n");
        exit(EXIT_FAILURE);
    }

    const char *duplicate_issue_id = NULL;

    if (duplicate) {
        if (optind + 1 >= argc) {
            printf("Error: --duplicate requires DUPLICATE_ISSUE_ID\n");
            exit(EXIT_FAILURE);
        }
        duplicate_issue_id = argv[optind + 1];
    }

    load_env("../src/.env");

    const char *org   = getenv("SENTRY_ORG");
    const char *token = getenv("SENTRY_TOKEN");

    if (!org || !token) {
        printf("Missing SENTRY_ORG or SENTRY_TOKEN in .env\n");
        exit(EXIT_FAILURE);
    }

    const char *issue_id = argv[optind];

    printf("Org: %s\n", org);
    printf("Token loaded\n");

    /* ── Fetch event list ──────────────────────────────────────────── */
    char *event_list_url = build_sentry_event_list_url(org, issue_id);
    if (!event_list_url) {
        fprintf(stderr, "Failed to build event-list URL\n");
        exit(EXIT_FAILURE);
    }

    char *sentry_issue_response = http_get_with_token(event_list_url, token);
    free(event_list_url);

    if (!sentry_issue_response) {
        fprintf(stderr, "Sentry event-list request failed\n");
        exit(EXIT_FAILURE);
    }

    char *all_events = process_event_list(sentry_issue_response,
                                          org, issue_id, token, detail);
    if (all_events && all_events[0] != '\0')
        printf("%s\n", all_events);

    if (claude) {
        char *prompt = duplicate
            ? build_duplicate_prompt(issue_id, duplicate_issue_id,
                                     sentry_issue_response, all_events)
            : build_analysis_prompt(issue_id,
                                    sentry_issue_response, all_events);

        if (!prompt) {
            fprintf(stderr, "Failed to build Claude prompt\n");
        } else {
            char *claude_response = run_claude_prompt(prompt);
            if (claude_response) {
                printf("Claude response:\n%s\n", claude_response);
                free(claude_response);
            }
            free(prompt);
        }
    }

    free(all_events);
    free(sentry_issue_response);

    return 0;
}
