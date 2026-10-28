#!/usr/bin/env bash
# Copy vendor/dw3_decomp into build/decomp-src and apply port/patches/*.patch.
# Usage:
#   tools/apply_patches.sh          # copy + apply
#   tools/apply_patches.sh --check  # verify pin, clean tree, patches apply
#   tools/apply_patches.sh --force  # copy + apply even when nothing changed
#
# Incremental (docs/BUILD_SPEED.md): the result is stamped with a hash of the
# pin, the patches and this script. When the stamp matches and nothing in the
# build copy was edited since, the copy is left alone. When the inputs did
# change, the new tree is built on the side and synced into place by content,
# so only the files whose bytes changed get a new mtime (and recompile).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC="${ROOT}/vendor/dw3_decomp"
DST="${ROOT}/build/decomp-src"
PIN_FILE="${ROOT}/port/patches/DECOMP_PIN"
PATCH_DIR="${ROOT}/port/patches"
MODE="apply"
FORCE=0

usage() {
  echo "Usage: $0 [--check]" >&2
  exit 2
}

if [[ $# -gt 1 ]]; then
  usage
fi
if [[ $# -eq 1 ]]; then
  case "$1" in
    --check) MODE="check" ;;
    --force) FORCE=1 ;;
    -h|--help) usage ;;
    *) usage ;;
  esac
fi

die() {
  echo "error: $*" >&2
  exit 1
}

require_src() {
  [[ -d "$SRC" ]] || die "missing submodule checkout: $SRC"
  [[ -e "${SRC}/.git" ]] || die "not a git checkout: $SRC (run: git submodule update --init)"
}

read_pin() {
  [[ -f "$PIN_FILE" ]] || die "missing pin file: $PIN_FILE"
  local pin
  pin="$(awk 'NF && $1 !~ /^#/ { print $1; exit }' "$PIN_FILE")"
  [[ -n "$pin" ]] || die "empty pin in $PIN_FILE"
  printf '%s\n' "$pin"
}

list_patches() {
  shopt -s nullglob
  local patches=("${PATCH_DIR}"/*.patch)
  shopt -u nullglob
  if [[ ${#patches[@]} -eq 0 ]]; then
    return 0
  fi
  printf '%s\n' "${patches[@]}" | LC_ALL=C sort
}

verify_pin_and_clean() {
  local expected actual
  expected="$(read_pin)"
  actual="$(git -C "$SRC" rev-parse HEAD)"
  if [[ "$actual" != "$expected" ]]; then
    die "submodule HEAD ${actual} != DECOMP_PIN ${expected}"
  fi
  if [[ -n "$(git -C "$SRC" status --porcelain)" ]]; then
    die "vendor/dw3_decomp is dirty; refuse to proceed"
  fi
}

copy_src_to() {
  local dest="$1"
  rm -rf "$dest"
  mkdir -p "$(dirname "$dest")"
  if command -v rsync >/dev/null 2>&1; then
    rsync -a --exclude='.git' "${SRC}/" "${dest}/"
  else
    mkdir -p "$dest"
    cp -a "${SRC}/." "${dest}/"
    rm -rf "${dest}/.git"
  fi
}

# git apply works outside a repository when run with cwd = tree root.
# Do not use `git -C` on the build copy (it has no .git by design).
git_apply_in() {
  local dest="$1"
  local check_flag="$2" # "" or "--check"
  local patch="$3"
  local ceiling
  ceiling="$(dirname "$dest")"
  if [[ -n "$check_flag" ]]; then
    (cd "$dest" && GIT_CEILING_DIRECTORIES="$ceiling" \
      git apply --check -- "$patch") \
      || die "patch failed --check: $patch"
  else
    echo "Applying $(basename "$patch")"
    (cd "$dest" && GIT_CEILING_DIRECTORIES="$ceiling" \
      git apply -- "$patch") \
      || die "patch failed to apply: $patch"
  fi
}

apply_patches_to() {
  local dest="$1"
  local check_only="${2:-0}"
  local patch
  local flag=""
  [[ "$check_only" -eq 1 ]] && flag="--check"
  while IFS= read -r patch; do
    [[ -n "$patch" ]] || continue
    git_apply_in "$dest" "$flag" "$patch"
  done < <(list_patches)
}

check_patches_with_git_apply() {
  local patches
  patches="$(list_patches || true)"
  if [[ -z "${patches}" ]]; then
    return 0
  fi

  local tmp
  tmp="$(mktemp -d "${TMPDIR:-/tmp}/dw3-patch-check.XXXXXX")"
  # Expand path now: a function closing over local tmp is unbound under
  # set -u when EXIT fires after a set -e failure (not die/exit).
  # shellcheck disable=SC2064
  trap "rm -rf -- '$tmp'" EXIT

  copy_src_to "$tmp"
  # Apply for real in order: later patches may stack on earlier ones
  # (e.g. 0010 on 0005's main.c). A pristine --check per file would reject
  # a valid series.
  apply_patches_to "$tmp" 0
  trap - EXIT
  rm -rf -- "$tmp"
}

inputs_hash() {
  {
    cat "$PIN_FILE" "${BASH_SOURCE[0]}"
    list_patches | while IFS= read -r p; do
      [[ -n "$p" ]] || continue
      basename "$p"
      cat "$p"
    done
  } | sha256sum | awk '{print $1}'
}

# Build the patched tree beside DST, then sync it into DST by content. The
# first run (no DST) is a plain rename.
apply_incremental() {
  local stage="${DST}.new"
  copy_src_to "$stage"
  apply_patches_to "$stage" 0
  if [[ -d "$DST" ]] && command -v rsync >/dev/null 2>&1; then
    # No -t: a file whose content changed gets mtime=now, so ninja rebuilds it;
    # identical files keep their old mtime. dw3-* stamps stay.
    rsync -rlc --delete --exclude='/.dw3-*' "${stage}/" "${DST}/"
    rm -rf "$stage"
  else
    rm -rf "$DST"
    mv "$stage" "$DST"
  fi
}

require_src

case "$MODE" in
  check)
    verify_pin_and_clean
    check_patches_with_git_apply
    echo "OK: pin matches, submodule clean, patches apply (empty series OK)"
    ;;
  apply)
    verify_pin_and_clean
    HASH_FILE="${DST}/.dw3-apply-hash"
    want="$(inputs_hash)"
    if [[ "$FORCE" -eq 0 && -f "$HASH_FILE" && "$(cat "$HASH_FILE")" == "$want" \
          && -z "$(find "$DST" -type f -newer "$HASH_FILE" ! -name '.dw3-*' -print -quit)" ]]; then
      echo "OK: ${DST} up to date (pin and patches unchanged)"
      exit 0
    fi
    echo "Copying ${SRC} -> ${DST}"
    apply_incremental
    printf '%s\n' "$want" > "$HASH_FILE"
    echo "OK: ${DST} ready"
    ;;
esac
