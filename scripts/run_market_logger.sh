#!/usr/bin/env bash

set -u

PROJECT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build"
LOG_FILE="${MARKET_LOG_FILE:-$PROJECT_DIR/market-data.log}"
LOCK_FILE="${TMPDIR:-/tmp}/hello-world-market-logger.lock"

mkdir -p "$(dirname -- "$LOG_FILE")"
exec 9>"$LOCK_FILE"
if ! flock -n 9; then
    exit 0
fi

while true; do
    {
        printf '\n[%s] market-data run\n' "$(date --iso-8601=seconds)"
        if [[ -x "$BUILD_DIR/fetch_btc_candles" ]]; then
            "$BUILD_DIR/fetch_btc_candles"
        else
            printf 'fetch_btc_candles is not built: %s\n' "$BUILD_DIR/fetch_btc_candles"
        fi

        if [[ -x "$BUILD_DIR/compare_btc_quotes" ]]; then
            "$BUILD_DIR/compare_btc_quotes"
        else
            printf 'compare_btc_quotes is not built: %s\n' "$BUILD_DIR/compare_btc_quotes"
        fi
    } >>"$LOG_FILE" 2>&1

    sleep 5
done
