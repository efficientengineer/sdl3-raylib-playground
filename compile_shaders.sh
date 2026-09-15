#!/bin/bash
set -e

SHADER_DIR=shaders
OUT_DIR=shaders/compiled
mkdir -p "$OUT_DIR"

for src in "$SHADER_DIR"/*.vert "$SHADER_DIR"/*.frag; do
    [ -f "$src" ] || continue
    base=$(basename "$src")
    name="${base%.*}"
    ext="${base##*.}"

    case "$ext" in
        vert) stage=vert ;;
        frag) stage=frag ;;
    esac

    spv="$OUT_DIR/${name}_${stage}.spv"
    glslangValidator -V "$src" -o "$spv" -S "$stage"

    spirv-cross "$spv" --output "$OUT_DIR/${name}_${stage}.glsl330" \
        --version 330
    spirv-cross "$spv" --output "$OUT_DIR/${name}_${stage}.glsl300es" \
        --version 300 --es
    spirv-cross "$spv" --output "$OUT_DIR/${name}_${stage}.msl" \
        --msl
    spirv-cross "$spv" --output "$OUT_DIR/${name}_${stage}.hlsl" \
        --hlsl --shader-model 50

    echo "  $base → spv, glsl330, glsl300es, msl, hlsl"
done

echo "Done. Output in $OUT_DIR/"
