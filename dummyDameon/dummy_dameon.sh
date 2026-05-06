#!/usr/bin/env bash
set -euo pipefail

: "${SERVICE_NAME:?SERVICE_NAME required}"
: "${TARGET_HOST:?TARGET_HOST required}"
: "${TARGET_PORT:?TARGET_PORT required}"
: "${LOG_FILE:?LOG_FILE required}"
EVENTS_PER_SEC="${EVENTS_PER_SEC:-1}"
ERROR_RATE="${ERROR_RATE:-0.1}"

[[ -r "$LOG_FILE" ]] || { echo "cannot read $LOG_FILE" >&2; exit 1; }

# pre-split the corpus into error and non-error pools
ERROR_POOL=$(mktemp)
NORMAL_POOL=$(mktemp)
trap 'rm -f "$ERROR_POOL" "$NORMAL_POOL"' EXIT

grep -iE 'error|fail|fatal|failure' "$LOG_FILE" > "$ERROR_POOL" || true
grep -ivE 'error|fail|fatal|failure' "$LOG_FILE" > "$NORMAL_POOL" || true

err_lines=$(wc -l < "$ERROR_POOL")
healthy_lines=$(wc -l < "$NORMAL_POOL")
[[ "$err_lines" -gt 0 ]] || { echo "no error lines in corpus" >&2; exit 1; }
[[ "$healthy_lines" -gt 0 ]] || { echo "no healthy lines in corpus" >&2; exit 1; }

interval=$(awk -v r="$EVENTS_PER_SEC" 'BEGIN{print 1/r}')

