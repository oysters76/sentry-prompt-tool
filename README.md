# sentry-prompt-tool

A command-line tool that fetches Sentry issue data, optionally enriches it
with per-event detail, and can pipe the collected context into **Claude** to
auto-generate a full incident-analysis document suite in Google Drive.

---

## Requirements

| Dependency | Notes |
|---|---|
| `gcc` ≥ 11 | Any C11-capable compiler works |
| [`libcurl`](https://curl.se/libcurl/) | HTTP requests to the Sentry API |
| [`cJSON`](https://github.com/DaveGamble/cJSON) | JSON parsing |
| [`claude` CLI](https://docs.claude.ai/en/docs/claude-code) | Required only for `--claude` mode |

Install dependencies on Debian / Ubuntu:

```bash
sudo apt install libcurl4-openssl-dev libcjson-dev
```

On macOS with Homebrew:

```bash
brew install curl cjson
```

---

## Building

```bash
make          # produces ./sentry-tool
make install  # copies to /usr/local/bin  (PREFIX= to override)
make clean    # removes object files
make distclean # removes object files + binary
```

---

## Configuration

Create a `.env` file in the directory from which you run the tool:

```ini
SENTRY_ORG=your-org-slug
SENTRY_TOKEN=sntrys_xxxxxxxxxxxxxxxxxxxx
```

The file is parsed at startup; existing environment variables are overwritten.

---

## Usage

```
sentry-tool [OPTIONS] ISSUE_ID [DUPLICATE_ISSUE_ID]
```

### Options

| Flag | Description |
|---|---|
| `-d`, `--detail` | Fetch full event detail JSON for every event in the issue |
| `-c`, `--claude` | Pipe collected data into Claude and print the response |
| `-u`, `--duplicate` | Document this issue as a duplicate of `DUPLICATE_ISSUE_ID` |

### Examples

**Fetch event list for an issue:**
```bash
./sentry-tool 12345678
```

**Fetch event list + full event detail:**
```bash
./sentry-tool --detail 12345678
```

**Fetch data and generate a Claude analysis document suite:**
```bash
./sentry-tool --detail --claude 12345678
```

**Document a duplicate issue:**
```bash
./sentry-tool --duplicate --claude 12345678 87654321
#                                  ^^^^^^^^ ^^^^^^^^
#                                  current  primary (duplicate of)
```

---

## Project Structure

```
.
├── main.c            # CLI entry point – argument parsing & orchestration
├── sentry_client.h   # Public API declarations
├── sentry_client.c   # All implementation (HTTP, JSON, prompt building)
├── Makefile
├── README.md
└── .env              # Not committed – create locally
```

---

## Claude Output

When `--claude` is supplied the tool builds a structured prompt and pipes it
through the `claude` CLI. Claude is instructed to create the following
documents inside `sentry/<ISSUE_ID>/` in your connected Google Drive:

| File | Purpose |
|---|---|
| `README.md` | Navigation hub and quick overview |
| `analysis.md` | Comprehensive root cause analysis |
| `quick-reference.md` | TL;DR for on-call engineers |
| `fix-implementation.md` | Full implementation guide with code, tests, deployment |
| `implementation-checklist.md` | Task-tracking checklist |
| `metadata.json` | Machine-readable incident data |

In `--duplicate` mode the document set is adjusted for duplicate-issue
documentation (`duplicate-analysis.md`, `resolution-guide.md`, etc.).

---

## License

MIT – see LICENSE file (add your own).
