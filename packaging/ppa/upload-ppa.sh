#!/usr/bin/env bash
# Builds signed source packages from the current commit and uploads them to a Launchpad PPA,
# one per Ubuntu release. Launchpad then builds the .debs.
#
# Usage: packaging/ppa/upload-ppa.sh ppa:<user>/<name> [series...]
#   e.g. packaging/ppa/upload-ppa.sh ppa:dynamo2k1/snapline noble resolute
#
# Needs: devscripts, dput, and a GPG key registered on Launchpad whose identity matches
# DEBFULLNAME / DEBEMAIL (export both before running).
set -euo pipefail

if [[ $# -lt 1 || $1 != ppa:* ]]; then
  echo "usage: $0 ppa:<user>/<name> [series...]" >&2
  exit 2
fi
ppa=$1
shift
series=("$@")
[[ ${#series[@]} -gt 0 ]] || series=(noble resolute)
: "${DEBFULLNAME:?export DEBFULLNAME=\"Your Name\" (as on your GPG key)}"
: "${DEBEMAIL:?export DEBEMAIL=you@example.com (as on your GPG key)}"

root="$(cd "$(dirname "$0")/../.." && pwd)"
version="$(dpkg-parsechangelog -l "$root/debian/changelog" -S Version)" # e.g. 0.2.0-1
upstream="${version%-*}"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

# One orig tarball shared by every series, so Launchpad accepts all uploads.
git -C "$root" archive --prefix="snapline-$upstream/" -o "$work/snapline_$upstream.orig.tar.gz" HEAD

for s in "${series[@]}"; do
  # ~ppa sorts below the plain version, so an official package would still win later.
  ppa_version="${version}~ppa1~${s}1"
  rm -rf "$work/snapline-$upstream"
  tar -xzf "$work/snapline_$upstream.orig.tar.gz" -C "$work"
  (
    cd "$work/snapline-$upstream"
    dch -b --newversion "$ppa_version" --distribution "$s" --force-distribution "PPA build for Ubuntu $s."
    debuild -S -sa -d
  )
  dput "$ppa" "$work/snapline_${ppa_version}_source.changes"
done

owner="${ppa#ppa:}"
echo "Uploaded. Watch the builds at https://launchpad.net/~${owner%%/*}/+archive/ubuntu/${ppa##*/}/+packages"
