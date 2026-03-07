#include "sentry_client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#include <curl/curl.h>
#include <cjson/cJSON.h>


/* libcurl write callback – appends received bytes to a Memory buffer. */
// cppcheck-suppress constParameterCallback
static size_t write_callback(void *contents, size_t size, size_t nmemb,
                              void *userp)
{
    size_t  realsize = size * nmemb;
    Memory *mem      = (Memory *)userp;

    char *ptr = realloc(mem->data, mem->size + realsize + 1);
    if (!ptr)
        return 0; /* signal OOM to curl */

    mem->data = ptr;
    memcpy(&mem->data[mem->size], contents, realsize);
    mem->size             += realsize;
    mem->data[mem->size]   = '\0';

    return realsize;
}

void load_env(const char *filename)
{
     if (!filename || filename[0] == '\0')
        filename = ".env";
     
    FILE *file = fopen(filename, "r");
/* Fallback: try .env next to the binary's working directory */
    if (!file && strcmp(filename, ".env") != 0) {
        fprintf(stderr, "load_env: could not open '%s', trying '.env'\n", filename);
        file = fopen(".env", "r");
    }
    if (!file) {
        perror(".env open failed");
        return;
    }

    char line[1024];

    while (fgets(line, sizeof(line), file)) {
        /* strip trailing newline */
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';

        /* skip comments and blank lines */
        if (line[0] == '#' || line[0] == '\0')
            continue;

        char *eq = strchr(line, '=');
        if (!eq)
            continue;

        *eq = '\0';
        setenv(line, eq + 1, 1);
    }

    fclose(file);
}

char *build_sentry_event_list_url(const char *org, const char *issue)
{
    static const char *tmpl =
        "https://sentry.io/api/0/organizations/%s/issues/%s/events/";

    int len = snprintf(NULL, 0, tmpl, org, issue);
    if (len < 0)
        return NULL;

    char *url = malloc((size_t)len + 1);
    if (!url)
        return NULL;

    snprintf(url, (size_t)len + 1, tmpl, org, issue);
    printf("url: %s\n", url);
    return url;
}

char *build_sentry_event_detail_url(const char *org,
                                    const char *issue,
                                    const char *event_id)
{
    static const char *tmpl =
        "https://sentry.io/api/0/organizations/%s/issues/%s/events/%s/";

    int len = snprintf(NULL, 0, tmpl, org, issue, event_id);
    if (len < 0)
        return NULL;
    
    char *url = malloc((size_t)len + 1);
    if (!url)
        return NULL;

    snprintf(url, (size_t)len + 1, tmpl, org, issue, event_id);
    printf("url: %s\n", url); 
    return url;
}

char *http_get_with_token(const char *url, const char *token)
{
    Memory chunk;
    chunk.data = malloc(1);
    if (!chunk.data)
        return NULL;
    chunk.data[0] = '\0';
    chunk.size    = 0;

    CURL *curl = curl_easy_init();
    if (!curl) {
        free(chunk.data);
        return NULL;
    }

    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s",
             token);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, auth_header);

    curl_easy_setopt(curl, CURLOPT_URL,           url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER,    headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,     (void *)&chunk); 

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "curl error: %s\n", curl_easy_strerror(res));
        free(chunk.data);
        chunk.data = NULL;
    }

    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);

    return chunk.data;
}

char *process_event_list(const char *json,
                         const char *org,
                         const char *issue_id,
                         const char *token,
                         bool        detail)
{
    cJSON *root = cJSON_Parse(json);
    if (!root) {
        fprintf(stderr, "JSON parse failed\n");
        return NULL; 
    }

    if (!cJSON_IsArray(root)) {
        fprintf(stderr, "Expected JSON array\n");
        cJSON_Delete(root);
        return NULL; 
    }

    char  *result      = malloc(1);
    size_t result_size = 0;

    if (!result) {
        cJSON_Delete(root);
        return NULL;
    }
    result[0] = '\0';

    int count = cJSON_GetArraySize(root);

    for (int i = 0; i < count; i++) {
        cJSON *event    = cJSON_GetArrayItem(root, i);
        cJSON *event_id = cJSON_GetObjectItem(event, "eventID");

        if (!cJSON_IsString(event_id))
            continue;

        printf("Event ID: %s\n", event_id->valuestring);

        if (!detail)
            continue;

        char *detail_url =
            build_sentry_event_detail_url(org, issue_id,
                                          event_id->valuestring);

        if (!detail_url)
            continue;

        char *detail_json = http_get_with_token(detail_url, token);
        free(detail_url);

        if (!detail_json)
            continue;  

        size_t len = strlen(detail_json);
        char  *tmp = realloc(result, result_size + len + 2);
        if (!tmp) {
            free(detail_json);
            break;
        }

        result = tmp;
        memcpy(result + result_size, detail_json, len);
        result_size += len;
        result[result_size++] = '\n';
        result[result_size]   = '\0';

        free(detail_json);
    }

    cJSON_Delete(root);
    return result;
}

char *build_analysis_prompt(const char *issue_id,
                            const char *sentry_issue_response,
                            const char *all_events)
{
    if (!sentry_issue_response) sentry_issue_response = "";
    if (!all_events)            all_events            = "";

    static const char *tmpl =
"create comprehensive analysis and implementation documents for the following Sentry error to be stored in Google Drive folder sentry/%s/\n"
"\n"
"---\n"
"\n"
"## Initial Analysis (Your Observations)\n"
"\n"
"Provide any initial thoughts on:\n"
"\n"
"1. What might be causing this?\n"
"2. Which component/service is responsible?\n"
"3. Any recent deployments or changes?\n"
"4. Is this blocking production?\n"
"\n"
"\n"
"%s %s\n"
"\n"
"---\n"
"\n"
"## Requirements\n"
"\n"
"Please create the following documents:\n"
"\n"
"1. **README.md**: Navigation hub and quick overview\n"
"2. **analysis.md**: Comprehensive root cause analysis\n"
"3. **quick-reference.md**: TL;DR for on-call engineers\n"
"4. **fix-implementation.md**: Detailed implementation guide with:\n"
"    - Solution architecture\n"
"    - Complete code changes with file paths and line numbers\n"
"    - Configuration updates\n"
"    - Testing strategy (unit, integration, manual tests)\n"
"    - Deployment plan with step-by-step instructions\n"
"    - Rollback procedures\n"
"    - Monitoring and alerting setup\n"
"5. **implementation-checklist.md**: Task tracking checklist\n"
"6. **metadata.json**: Machine-readable incident data\n"
"\n"
"## Output Structure\n"
"\n"
"Create documents in: sentry/%s\n"
"\n"
"Ensure the fix-implementation.md includes:\n"
"\n"
"- Actual code examples from the codebase (if you can find the relevant files)\n"
"- Specific file paths based on the project structure\n"
"- Complete test examples that can be copy-pasted\n"
"- SQL queries for data investigation\n"
"- Configuration examples with environment variables\n"
"- Deployment commands specific to the infrastructure (K8s, Docker, etc.)\n";

    int len = snprintf(NULL, 0, tmpl,
                       issue_id, sentry_issue_response, all_events, issue_id);
    if (len < 0)
        return NULL;

    char *prompt = malloc((size_t)len + 1);
    if (!prompt)
        return NULL;

    snprintf(prompt, (size_t)len + 1, tmpl,
             issue_id, sentry_issue_response, all_events, issue_id);
    return prompt;
}

char *build_duplicate_prompt(const char *issue_id,
                             const char *duplicate_issue_id,
                             const char *sentry_issue_response,
                             const char *all_events)
{
    if (!sentry_issue_response) sentry_issue_response = "";
    if (!all_events)            all_events            = "";

    static const char *tmpl =
"create comprehensive duplicate issue documentation for the following Sentry error.\n"
"\n"
"The issue `%s` has been identified as a duplicate of `%s`.\n"
"\n"
"All documentation should be stored in the Google Drive folder:\n"
"\n"
"sentry/%s/\n"
"\n"
"---\n"
"\n"
"## Initial Analysis (Your Observations)\n"
"\n"
"Provide any initial thoughts on:\n"
"\n"
"1. Why is this issue considered a duplicate?\n"
"2. What similarities exist between the two issues?\n"
"3. Are the root causes identical or just symptoms?\n"
"4. Should the duplicate issue be closed or merged?\n"
"\n"
"**Example context from Sentry:**\n"
"\n"
"%s %s\n"
"\n"
"---\n"
"\n"
"## Requirements\n"
"\n"
"Please create the following documents:\n"
"\n"
"1. **README.md**: Overview of the duplicate relationship\n"
"2. **duplicate-analysis.md**: Explanation of why `%s` duplicates `%s`\n"
"3. **quick-reference.md**: TL;DR for engineers encountering this issue\n"
"4. **resolution-guide.md**: Steps engineers should take when this duplicate occurs\n"
"5. **implementation-checklist.md**: Checklist for resolving and closing duplicates\n"
"6. **metadata.json**: Machine-readable incident metadata\n"
"\n"
"## Output Structure\n"
"\n"
"Create documents in: sentry/%s\n"
"\n"
"The duplicate-analysis.md should include:\n"
"\n"
"- Comparison of stack traces\n"
"- Comparison of error messages\n"
"- Shared root cause explanation\n"
"- Explanation of why this issue should be closed as duplicate\n"
"- References to the primary issue `%s`\n"
"\n"
"Include:\n"
"\n"
"- Code references if relevant\n"
"- Configuration context\n"
"- Database queries that help confirm duplication\n"
"- Observability signals (logs, metrics, traces)\n";

    int len = snprintf(NULL, 0, tmpl,
                       issue_id, duplicate_issue_id,
                       issue_id,
                       sentry_issue_response, all_events,
                       issue_id, duplicate_issue_id,
                       issue_id,
                       duplicate_issue_id);
    if (len < 0)
        return NULL;

    char *prompt = malloc((size_t)len + 1);
    if (!prompt)
        return NULL;

    snprintf(prompt, (size_t)len + 1, tmpl,
             issue_id, duplicate_issue_id,
             issue_id,
             sentry_issue_response, all_events,
             issue_id, duplicate_issue_id,
             issue_id,
             duplicate_issue_id);
    return prompt;
}

char *run_claude_prompt(const char *prompt)
{
  char tmp_path[] = "/tmp/sentry_claude_XXXXXX";
    int  fd         = mkstemp(tmp_path);
    if (fd < 0) {
        perror("mkstemp");
        return "";
    }

    FILE *tmp = fdopen(fd, "w");
    if (!tmp) {
        perror("fdopen");
        close(fd);
        return "";
    }
    fputs(prompt, tmp);
    fclose(tmp);

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        remove(tmp_path);
        return "";
    }

    if (pid == 0) {
        /* child: exec claude directly, inherits stdin/stdout/stderr */
        char *const argv[] = { "claude", prompt, NULL };
        execvp("claude", argv);
        perror("execvp: claude");
        _exit(1);
    }

    /* parent: wait for claude to finish */
    waitpid(pid, NULL, 0);
    remove(tmp_path);
    return "success";
}
