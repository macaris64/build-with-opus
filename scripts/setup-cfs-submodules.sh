#!/usr/bin/env bash
# setup-cfs-submodules.sh — Initialize NASA cFS git submodules for Phase C.
#
# Run once from the repo root AFTER getting explicit approval to vendor
# cFE, OSAL, and PSP as git submodules:
#
#   bash scripts/setup-cfs-submodules.sh
#
# This script is NOT run automatically by CI. The cfs-runtime CI job checks
# for cfs/cFE/CMakeLists.txt and auto-skips if the submodule is absent.
#
# Pinned tag policy: always pin to the most recent stable release tag.
# Check https://github.com/nasa/cFE/releases for the latest.
#
# IMPORTANT: Adding these submodules requires network access and will add
# approximately 50 MB to the repository checkout.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${REPO_ROOT}"

CFE_TAG="v6.8.0"
OSAL_TAG="v6.3.0"
PSP_TAG="v1.4.0"

echo "Adding cFE submodule (tag ${CFE_TAG})..."
git submodule add --depth 1 https://github.com/nasa/cFE.git cfs/cFE
git -C cfs/cFE checkout "${CFE_TAG}"

echo "Adding OSAL submodule (tag ${OSAL_TAG})..."
git submodule add --depth 1 https://github.com/nasa/osal.git cfs/osal
git -C cfs/osal checkout "${OSAL_TAG}"

echo "Adding PSP submodule (tag ${PSP_TAG})..."
git submodule add --depth 1 https://github.com/nasa/PSP.git cfs/PSP
git -C cfs/PSP checkout "${PSP_TAG}"

echo ""
echo "Submodules initialized. Build with cFE runtime:"
echo "  cmake -B build_cfs -DSAKURA_CFS_RUNTIME=ON -DCMAKE_BUILD_TYPE=Debug"
echo "  cmake --build build_cfs"
