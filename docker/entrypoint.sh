#!/usr/bin/env bash
set -euo pipefail

MODE="${1:-backend}"

if [[ "$MODE" == "backend" ]]; then
  exec python3 -m depth_anything_3.services.backend \
    --model-dir "${DA3_MODEL_DIR}" \
    --device "${DA3_DEVICE}" \
    --host "${DA3_HOST}" \
    --port "${DA3_PORT}"
fi

exec "$@"
