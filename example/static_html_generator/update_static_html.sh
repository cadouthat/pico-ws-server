#!/usr/bin/env bash
# Rebuild static.html.gz and static_html_hex.h from Terminal/index.html.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
HTML_FILE="${SCRIPT_DIR}/index.html"
GZIP_FILE="${REPO_ROOT}/static.html.gz"
HEADER_FILE="${REPO_ROOT}/static_html_hex.h"

if ! command -v gzip >/dev/null 2>&1; then
  echo "Error: gzip is required but not installed." >&2
  exit 1
fi

if ! command -v xxd >/dev/null 2>&1; then
  echo "Error: xxd is required but not installed." >&2
  exit 1
fi

echo "Compressing ${HTML_FILE} -> ${GZIP_FILE}" && \
  gzip --best -c "${HTML_FILE}" > "${GZIP_FILE}"

echo "Generating ${HEADER_FILE}" && \
  {
    printf '/* Auto-generated from Terminal/index.html */\n'
    printf '#ifndef STATIC_HTML_HEX_H\n#define STATIC_HTML_HEX_H\n\n'
    printf '#include <stddef.h>\n\n'
    # xxd -n sets a stable symbol name regardless of path; keep aligned buffer and const length
    xxd -i -n static_html_gz "${GZIP_FILE}" | \
      sed 's/unsigned char static_html_gz\[\]/static const unsigned char static_html_gz[] __attribute__((aligned(4)))/' | \
      sed 's/unsigned int static_html_gz_len/static const unsigned int static_html_gz_len/'
    printf '\n#endif /* STATIC_HTML_HEX_H */\n'
  } > "${HEADER_FILE}"

rm -f "${GZIP_FILE}"
