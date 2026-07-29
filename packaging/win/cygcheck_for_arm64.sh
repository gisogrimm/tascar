#!/bin/bash
# Replacement for cygcheck for msys2 CLANGARM64 TASCAR. We need to use
# llvm-objdump -p ./$file | grep "DLL Name: "
# and we need to apply it recursively.

declare -A seen
declare -a result

scan() {
  local filename="$1"

  # Normalize to avoid mismatched whitespace/paths
  # (if callers ever pass relative paths, resolve them)
  if [[ "$filename" != /* ]]; then
    filename="$(cd "$(dirname "$filename")" && pwd)/$(basename "$filename")"
  fi

  if [[ -n "${seen["$filename"]}" ]]; then
    return 0
  fi
  seen["$filename"]=1

  # Dump direct DLL deps
  # Note: llvm-objdump output has "DLL Name: <name>"
  # If the grep yields nothing, there are no dependencies.
  local dependencies
  dependencies="$(llvm-objdump -p "$filename" 2>/dev/null | awk '/DLL Name:/{print $3}')"

  local dependency
  for dependency in $dependencies; do
    if [[ -f "/clangarm64/bin/$dependency" ]]; then
      local full="/clangarm64/bin/$dependency"
      result+=("$full")
      scan "$full"
    fi
  done
}

for start_filename in "$@"; do
  scan "$start_filename"
done
printf '%s\n' "${result[@]}"
