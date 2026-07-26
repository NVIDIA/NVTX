#!/usr/bin/env bash

# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

set -euo pipefail

if (($# != 1)); then
    echo "Usage: $0 <true|false>" >&2
    exit 2
fi

publisher_upload="$1"
case "$publisher_upload" in
    true | false)
        ;;
    *)
        echo "Upload mode must be true or false." >&2
        exit 2
        ;;
esac

: "${PYTHON_PUBLISHER_RELEASE_API_URL:?release API URL must be set}"
: "${PYTHON_PUBLISHER_STATUS_API_URL:?status API URL must be set}"
: "${PYTHON_PUBLISHER_API_TOKEN:?publisher ID token must be set}"
: "${PYTHON_PUBLISHER_PIC:?publisher PIC email must be set}"

release_request="$(
    jq --null-input \
        --argjson upload "$publisher_upload" \
        --arg pic "$PYTHON_PUBLISHER_PIC" \
        --slurpfile sdist_upload artifactory-sdist-upload.json \
        --slurpfile wheels_upload artifactory-wheels-upload.json \
        '{
          project_name: "nvtx",
          payload: (
            [$sdist_upload[0].files[], $wheels_upload[0].files[]]
            | map({
                pic: $pic,
                job_type: "wheel-release-job",
                url: .target,
                upload: $upload
              })
          )
        }'
)"

curl --fail-with-body --silent --show-error \
    --connect-timeout 30 \
    --max-time 120 \
    --request POST \
    "$PYTHON_PUBLISHER_RELEASE_API_URL" \
    --header "Authorization: Bearer ${PYTHON_PUBLISHER_API_TOKEN}" \
    --header "Content-Type: application/json" \
    --data "$release_request" \
    --output publisher-release.json

release_uuid="$(
    jq --exit-status --raw-output \
        '.release_uuid' publisher-release.json
)"
release_status="$(
    jq --exit-status --raw-output \
        '.status' publisher-release.json
)"
printf 'Publisher release %s accepted with status %s.\n' \
    "$release_uuid" "$release_status"

poll_deadline=$((SECONDS + 3600))

while ((SECONDS < poll_deadline)); do
    curl --fail-with-body --silent --show-error \
        --connect-timeout 30 \
        --max-time 120 \
        --retry 3 \
        --retry-delay 5 \
        --header "Authorization: Bearer ${PYTHON_PUBLISHER_API_TOKEN}" \
        --output publisher-status.json \
        "$PYTHON_PUBLISHER_STATUS_API_URL/$release_uuid"

    release_status="$(
        jq --exit-status --raw-output \
            '.status' publisher-status.json
    )"
    printf 'Publisher release %s has status %s.\n' \
        "$release_uuid" "$release_status"

    case "$release_status" in
        completed)
            exit 0
            ;;
        failed)
            printf 'Publisher release %s failed.\n' "$release_uuid" >&2
            exit 1
            ;;
    esac

    sleep 30
done

printf 'Timed out waiting for publisher release %s.\n' "$release_uuid" >&2
exit 1
