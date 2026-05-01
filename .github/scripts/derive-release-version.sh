#!/bin/bash
set -e

REF="${1:?Usage: $0 <ref>}"

UPSTREAM_LIBFABRIC_REPO="${UPSTREAM_LIBFABRIC_REPO:-https://github.com/ofiwg/libfabric.git}"
AWS_LIBFABRIC_REPO="${AWS_LIBFABRIC_REPO:-https://github.com/aws/libfabric.git}"

if [[ "${REF}" =~ ^v?([0-9]+)\.([0-9]+)\.([0-9]+)$ ]]; then
    VERSION_PREFIX="${BASH_REMATCH[1]}.${BASH_REMATCH[2]}"
    BASE_UPSTREAM_TAG="v${BASH_REMATCH[1]}.${BASH_REMATCH[2]}.${BASH_REMATCH[3]}"

    if ! git ls-remote --exit-code --tags "${UPSTREAM_LIBFABRIC_REPO}" "refs/tags/${BASE_UPSTREAM_TAG}" >/dev/null 2>&1; then
        echo "Error: upstream tag '${BASE_UPSTREAM_TAG}' does not exist in ${UPSTREAM_LIBFABRIC_REPO}." >&2
        echo "Use version_override to specify a version manually." >&2
        exit 1
    fi
    echo "Using exact upstream release tag from ref: ${BASE_UPSTREAM_TAG#v}" >&2
elif [[ "${REF}" =~ ^v?([0-9]+\.[0-9]+)\.x$ ]]; then
    VERSION_PREFIX="${BASH_REMATCH[1]}"
    BASE_UPSTREAM_TAG=$(git ls-remote --tags "${UPSTREAM_LIBFABRIC_REPO}" \
        | awk -F'/' '{print $3}' \
        | grep -v '\^{}' \
        | grep -E "^v${VERSION_PREFIX}\.[0-9]+$" \
        | sort -V \
        | tail -1)

    if [ -z "${BASE_UPSTREAM_TAG}" ]; then
        echo "Error: no upstream release found for ${VERSION_PREFIX}.x series." >&2
        echo "Use version_override to specify a version manually." >&2
        exit 1
    fi
    echo "Latest upstream release of ${VERSION_PREFIX}.x: ${BASE_UPSTREAM_TAG#v}" >&2
else
    VERSION_PREFIX=$(grep 'AC_INIT' configure.ac \
        | sed 's/.*\[libfabric\], \[\([^]]*\)\].*/\1/' \
        | grep -oE '^[0-9]+\.[0-9]+')
    if [ -z "${VERSION_PREFIX}" ]; then
        echo "Error: cannot determine version prefix from ref '${REF}' or configure.ac" >&2
        exit 1
    fi

    # For SHA or non-series refs, anchor on the newest upstream release tag
    # reachable from the checked-out commit to keep version and source aligned.
    git fetch --tags --quiet "${UPSTREAM_LIBFABRIC_REPO}"
    BASE_UPSTREAM_TAG=$(git tag --merged HEAD --list "v${VERSION_PREFIX}.*" \
        | grep -E "^v${VERSION_PREFIX}\.[0-9]+$" \
        | sort -V \
        | tail -1)

    if [ -z "${BASE_UPSTREAM_TAG}" ]; then
        echo "Error: no upstream ${VERSION_PREFIX}.x release tag is reachable from '${REF}'." >&2
        echo "Use an exact upstream tag (vX.Y.Z), a series branch (vX.Y.x), or version_override." >&2
        exit 1
    fi
    echo "Latest upstream release reachable from ${REF} in ${VERSION_PREFIX}.x: ${BASE_UPSTREAM_TAG#v}" >&2
fi

LATEST_UPSTREAM_VERSION="${BASE_UPSTREAM_TAG#v}"

LATEST_AMZN_TAG=$(git ls-remote --tags "${AWS_LIBFABRIC_REPO}" \
    | awk -F'/' '{print $3}' \
    | grep -v '\^{}' \
    | grep -E "^${BASE_UPSTREAM_TAG}amzn[0-9]+\.0$" \
    | sort -V \
    | tail -1)
LATEST_AMZN_VERSION="${LATEST_AMZN_TAG#v}"
echo "Latest AWS release on top of ${LATEST_UPSTREAM_VERSION}: ${LATEST_AMZN_VERSION:-"(none - first AWS release on this upstream tag)"}" >&2

if [ -z "${LATEST_AMZN_VERSION}" ]; then
    RELEASE_VERSION="${LATEST_UPSTREAM_VERSION}amzn1.0"
else
    AMZN_NUM=$(echo "${LATEST_AMZN_VERSION}" | grep -oE 'amzn[0-9]+' | grep -oE '[0-9]+')
    RELEASE_VERSION="${LATEST_UPSTREAM_VERSION}amzn$((AMZN_NUM + 1)).0"
fi

echo "New AWS release version: ${RELEASE_VERSION}"
