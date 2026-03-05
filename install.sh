#!/usr/bin/env bash
# ──────────────────────────────────────────────────────────────────────
# install.sh  –  Download and install sentry-tool from GitHub Releases
#
# Usage:
#   curl -sSL https://raw.githubusercontent.com/YOUR_ORG/sentry-tool/main/install.sh | bash
#
# Options (environment variables):
#   INSTALL_DIR   – destination directory  (default: /usr/local/bin)
#   VERSION       – release tag to install (default: latest)
#   REPO          – GitHub repo slug       (default: YOUR_ORG/sentry-tool)
# ──────────────────────────────────────────────────────────────────────
set -euo pipefail

REPO="${REPO:-YOUR_ORG/sentry-tool}"
INSTALL_DIR="${INSTALL_DIR:-/usr/local/bin}"
BINARY_NAME="sentry-tool"

# ── Colours ───────────────────────────────────────────────────────────
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
BOLD='\033[1m'; RESET='\033[0m'

info()    { echo -e "${BOLD}[sentry-tool]${RESET} $*"; }
success() { echo -e "${GREEN}✔${RESET} $*"; }
warn()    { echo -e "${YELLOW}⚠${RESET}  $*"; }
die()     { echo -e "${RED}✖${RESET}  $*" >&2; exit 1; }

# ── Detect architecture ───────────────────────────────────────────────
detect_arch() {
  local machine
  machine="$(uname -m)"
  case "$machine" in
    x86_64|amd64)          echo "linux-amd64"  ;;
    aarch64|arm64)         echo "linux-arm64"  ;;
    armv7*|armhf)          echo "linux-armv7"  ;;
    *)                     die "Unsupported architecture: $machine" ;;
  esac
}

# ── Resolve latest version from GitHub API ───────────────────────────
latest_version() {
  if command -v curl &>/dev/null; then
    curl -sSf "https://api.github.com/repos/${REPO}/releases/latest" \
      | grep '"tag_name"' \
      | head -1 \
      | sed 's/.*"tag_name": *"\([^"]*\)".*/\1/'
  elif command -v wget &>/dev/null; then
    wget -qO- "https://api.github.com/repos/${REPO}/releases/latest" \
      | grep '"tag_name"' \
      | head -1 \
      | sed 's/.*"tag_name": *"\([^"]*\)".*/\1/'
  else
    die "curl or wget is required"
  fi
}

# ── Download helper ───────────────────────────────────────────────────
download() {
  local url="$1" dest="$2"
  if command -v curl &>/dev/null; then
    curl -sSfL "$url" -o "$dest"
  else
    wget -qO "$dest" "$url"
  fi
}

# ── Main ──────────────────────────────────────────────────────────────
main() {
  info "Detecting platform…"
  ARCH="$(detect_arch)"
  success "Architecture: $ARCH"

  VERSION="${VERSION:-}"
  if [ -z "$VERSION" ]; then
    info "Resolving latest release…"
    VERSION="$(latest_version)"
    [ -n "$VERSION" ] || die "Could not determine latest version"
  fi
  success "Version: $VERSION"

  BASE_URL="https://github.com/${REPO}/releases/download/${VERSION}"
  BINARY_FILE="${BINARY_NAME}-${ARCH}"
  DOWNLOAD_URL="${BASE_URL}/${BINARY_FILE}"
  CHECKSUM_URL="${BASE_URL}/SHA256SUMS.txt"

  TMP_DIR="$(mktemp -d)"
  trap 'rm -rf "$TMP_DIR"' EXIT

  info "Downloading ${BINARY_FILE}…"
  download "$DOWNLOAD_URL" "${TMP_DIR}/${BINARY_NAME}"

  info "Verifying checksum…"
  download "$CHECKSUM_URL" "${TMP_DIR}/SHA256SUMS.txt"
  # Rewrite checksum file to match the local filename
  sed "s/${BINARY_FILE}/${BINARY_NAME}/" "${TMP_DIR}/SHA256SUMS.txt" \
    > "${TMP_DIR}/SHA256SUMS_local.txt"
  ( cd "$TMP_DIR" && sha256sum --check --ignore-missing SHA256SUMS_local.txt ) \
    || die "Checksum verification failed — download may be corrupt"
  success "Checksum verified"

  chmod +x "${TMP_DIR}/${BINARY_NAME}"

  # Install — use sudo if INSTALL_DIR isn't writable by current user
  info "Installing to ${INSTALL_DIR}/${BINARY_NAME}…"
  mkdir -p "$INSTALL_DIR"
  if [ -w "$INSTALL_DIR" ]; then
    mv "${TMP_DIR}/${BINARY_NAME}" "${INSTALL_DIR}/${BINARY_NAME}"
  else
    warn "Requesting sudo to write to ${INSTALL_DIR}"
    sudo mv "${TMP_DIR}/${BINARY_NAME}" "${INSTALL_DIR}/${BINARY_NAME}"
  fi

  success "Installed ${BINARY_NAME} ${VERSION} → ${INSTALL_DIR}/${BINARY_NAME}"

  # Sanity check
  if command -v "$BINARY_NAME" &>/dev/null; then
    success "$(${BINARY_NAME} --version 2>/dev/null || echo 'Run: sentry-tool --help')"
  else
    warn "${INSTALL_DIR} may not be in your PATH."
    warn "Add this to your shell profile:  export PATH=\"${INSTALL_DIR}:\$PATH\""
  fi
}

main "$@"
