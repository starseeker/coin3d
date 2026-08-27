#!/usr/bin/env bash

# Convert SVG assets to small GIF textures, resolve them through an Inventor
# scene, and emit C++ string arrays containing the inlined scene data.

set -euo pipefail

if (( $# < 2 )); then
  echo "usage: $0 OUTPUT.cpp INPUT.svg [INPUT.svg ...]" >&2
  exit 2
fi

output=$1
shift
inline_texture=${INLINE_TEXTURE:-inline_texture}

if ! command -v "$inline_texture" >/dev/null 2>&1; then
  echo "inventify_texturenames: '$inline_texture' was not found" >&2
  echo "build with -DOBOL_BUILD_TOOLS=ON or set INLINE_TEXTURE to its path" >&2
  exit 1
fi

if command -v magick >/dev/null 2>&1; then
  image_convert=(magick)
elif command -v convert >/dev/null 2>&1; then
  image_convert=(convert)
else
  echo "inventify_texturenames: ImageMagick (magick or convert) is required" >&2
  exit 1
fi

workdir=$(mktemp -d "${TMPDIR:-/tmp}/obol-inventify.XXXXXX")
trap 'rm -rf -- "$workdir"' EXIT
: > "$output"

for source in "$@"; do
  if [[ ! -f "$source" ]]; then
    echo "inventify_texturenames: input does not exist: $source" >&2
    exit 1
  fi

  stem=${source%.*}
  gif="$workdir/$(basename "$stem").gif"
  scene="$workdir/texture.iv"
  inlined="$workdir/inlined.iv"
  identifier=$(basename "$stem" | tr -c '[:alnum:]_' '_')
  [[ $identifier =~ ^[0-9] ]] && identifier="texture_$identifier"

  echo "Generating source for $source"
  "${image_convert[@]}" "$source" -colors 2 -scale 128x128 "$gif"
  printf '#Inventor V2.1 ascii\nTexture2 { filename "%s" }\n' "$gif" > "$scene"
  "$inline_texture" < "$scene" > "$inlined"

  printf 'static const char * const %s[] = {\n' "$identifier" >> "$output"
  while IFS= read -r line || [[ -n $line ]]; do
    line=${line//\\/\\\\}
    line=${line//\"/\\\"}
    printf '  "%s\\n",\n' "$line" >> "$output"
  done < "$inlined"
  printf '  nullptr\n};\n\n' >> "$output"
done
