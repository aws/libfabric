#!/bin/bash
# Renders the release body text to stdout.
#
# Environment variables:
#   RELEASE_VERSION              - release version (e.g., 2.4.0amzn1.0 or 2.4.0 for ofiwg-release builds)
#   UPSTREAM_REPO                - upstream repo in owner/repo format
#   UPSTREAM_SHA                 - upstream commit SHA the release is built from
#   RELEASE_REPO                 - release repo in owner/repo format
#   SERVER_URL                   - GitHub server URL (e.g., https://github.com)
#   PREV_TAG                     - previous AWS release tag (optional)
#   CURRENT_TAG                  - current release tag (e.g., v2.4.0amzn1.0 or v2.4.0)
#   BUILD_TYPE                   - 'ofiwg-release' or 'aws-patched' (defaults to aws-patched if unset)
#   BUILD_ATTESTATION_URL        - build provenance attestation URL (use '[link generated at publish time]' for preview)
#   SOURCE_BINDING_ATTESTATION_URL - source-binding attestation URL (use '[link generated at publish time]' for preview)

set -e

: "${RELEASE_VERSION:?RELEASE_VERSION is required}"
: "${UPSTREAM_REPO:?UPSTREAM_REPO is required}"
: "${UPSTREAM_SHA:?UPSTREAM_SHA is required}"
: "${RELEASE_REPO:?RELEASE_REPO is required}"
: "${SERVER_URL:?SERVER_URL is required}"
: "${CURRENT_TAG:?CURRENT_TAG is required}"
: "${BUILD_ATTESTATION_URL:?BUILD_ATTESTATION_URL is required}"
: "${SOURCE_BINDING_ATTESTATION_URL:?SOURCE_BINDING_ATTESTATION_URL is required}"

BUILD_TYPE="${BUILD_TYPE:-aws-patched}"

if [ "${BUILD_TYPE}" = "ofiwg-release" ]; then
    cat <<EOF
Build of unmodified upstream release ${UPSTREAM_REPO}@${UPSTREAM_SHA} (${CURRENT_TAG}).

The source in this release is identical to [${UPSTREAM_REPO}@${CURRENT_TAG}](${SERVER_URL}/${UPSTREAM_REPO}/releases/tag/${CURRENT_TAG}). The build artifacts are produced by AWS and carry AWS-generated attestations.
EOF
else
    cat <<EOF
This release is based on ${UPSTREAM_REPO}@${UPSTREAM_SHA}
EOF
fi

if [ -n "${PREV_TAG}" ]; then
    echo "**Full Changelog**: ${SERVER_URL}/${RELEASE_REPO}/compare/${PREV_TAG}...${CURRENT_TAG}"
fi

cat <<EOF

**Provenance**: All release artifacts include [sigstore build provenance attestations](${BUILD_ATTESTATION_URL}). Verify with:
\`\`\`
gh attestation verify <artifact> --repo ${RELEASE_REPO}
\`\`\`
**Source binding attestation**: Includes upstream source binding metadata (repo/ref/resolved commit and artifact SHA256 digests). [View](${SOURCE_BINDING_ATTESTATION_URL})
EOF
