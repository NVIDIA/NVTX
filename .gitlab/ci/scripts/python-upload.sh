#!/usr/bin/env bash

# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

set -euo pipefail

: "${ARTIFACTORY_URL:?Artifactory URL must be set}"
: "${ARTIFACTORY_OIDC_AUDIENCE:?OIDC audience must be set}"
: "${ARTIFACTORY_OIDC_PROVIDER_NAME:?OIDC provider must be set}"
: "${ARTIFACTORY_PYPI_REPOSITORY:?PyPI repository must be set}"

jf_config_name="setup-jfrog-gitlab-ci-server-${CI_JOB_ID}"

cleanup_jfrog_config() {
    local status=$?

    trap - EXIT
    jf config remove "$jf_config_name" --quiet || true
    exit "$status"
}

trap cleanup_jfrog_config EXIT

jf config add "$jf_config_name" \
    --overwrite \
    --url="$ARTIFACTORY_URL" \
    --oidc-provider-name="$ARTIFACTORY_OIDC_PROVIDER_NAME" \
    --interactive=false

sdist_filename="$(basename python/dist/nvtx-*.tar.gz)"
package_version="${sdist_filename#nvtx-}"
package_version="${package_version%.tar.gz}"
upload_path="nvtx/$package_version"
if [[ "$CI_COMMIT_BRANCH" != "release-v3" ]]; then
    upload_path="$upload_path/$CI_COMMIT_BRANCH/$CI_COMMIT_SHA"
fi

jf rt upload \
    "python/dist/*.tar.gz" \
    "$ARTIFACTORY_PYPI_REPOSITORY/$upload_path/" \
    --flat=true \
    --fail-no-op=true \
    --format json \
    > artifactory-sdist-upload.json

jf rt upload \
    "wheelhouse/*.whl" \
    "$ARTIFACTORY_PYPI_REPOSITORY/$upload_path/" \
    --flat=true \
    --fail-no-op=true \
    --format json \
    > artifactory-wheels-upload.json
