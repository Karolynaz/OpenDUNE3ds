#!/bin/bash
set -e

BUILD_DIR="test builds"
mkdir -p "$BUILD_DIR"

# If an existing build is present in "test builds/", archive it to the next vXX folder
if [ -f "$BUILD_DIR/OpenDUNE3DS.3dsx" ]; then
    V_NUM=1
    while true; do
        V_DIR=$(printf "%s/v%02d" "$BUILD_DIR" "$V_NUM")
        if [ ! -d "$V_DIR" ]; then
            mkdir -p "$V_DIR"
            echo "Archiving previous build to $V_DIR/..."
            [ -f "$BUILD_DIR/OpenDUNE3DS.3dsx" ] && mv "$BUILD_DIR/OpenDUNE3DS.3dsx" "$V_DIR/"
            [ -f "$BUILD_DIR/OpenDUNE3DS.cia" ]  && mv "$BUILD_DIR/OpenDUNE3DS.cia"  "$V_DIR/"
            [ -f "$BUILD_DIR/OpenDUNE3DS.smdh" ] && mv "$BUILD_DIR/OpenDUNE3DS.smdh" "$V_DIR/"
            break
        fi
        V_NUM=$((V_NUM + 1))
    done
fi

# Copy the latest build into "test builds/"
if [ -f "OpenDUNE3DS.3dsx" ]; then
    cp OpenDUNE3DS.3dsx "$BUILD_DIR/"
    echo "Copied OpenDUNE3DS.3dsx -> $BUILD_DIR/"
fi

if [ -f "OpenDUNE3DS.cia" ]; then
    cp OpenDUNE3DS.cia "$BUILD_DIR/"
    echo "Copied OpenDUNE3DS.cia -> $BUILD_DIR/"
fi

if [ -f "OpenDUNE3DS.smdh" ]; then
    cp OpenDUNE3DS.smdh "$BUILD_DIR/"
fi

echo "Latest test build is ready in: $BUILD_DIR/"
