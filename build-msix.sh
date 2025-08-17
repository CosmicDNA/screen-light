#!/bin/bash

#
# Builds the ScreenLight project, prepares all necessary files, and creates a
# signed MSIX package. This script is designed to be run from within WSL2.
#

set -e # Exit immediately if a command exits with a non-zero status.

# --- Configuration & Help ---
usage() {
    echo "Usage: $0 -o <OutputDir> [-c <PfxFile>]"
    echo "  -o: Path where the final signed MSIX package will be saved (WSL path)."
    echo "  -c: (Optional) Path to your .pfx code signing certificate (WSL path)."
    echo "      If omitted, the package will be created but not signed."
    exit 1
}

get-version() {
  local BASE_PATH="$1"
  ls -1 "$BASE_PATH" | grep -E '^[0-9]'  | sort -V | tail -n 1
}

# --- Parse Command-Line Arguments ---
while getopts "o:c:" opt; do
    case ${opt} in
        o) OUTPUT_DIR=$OPTARG ;;
        c) PFX_FILE=$OPTARG ;;
        *) usage ;;
    esac
done

if [ -z "$OUTPUT_DIR" ]; then
    usage
fi

# --- Set up Paths ---
SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)
PROJECT_ROOT=$SCRIPT_DIR
STAGING_DIR="$PROJECT_ROOT/build/msix_staging"

# --- Find Windows SDK Tools from WSL ---
echo "--- Searching for Windows SDK tools..."
SDK_RAW_PATH="/mnt/c/Program Files (x86)/Windows Kits"
SDK_BASE_PATH="$SDK_RAW_PATH/$(get-version "$SDK_RAW_PATH")/bin"

if [ ! -d "$SDK_BASE_PATH" ]; then
    echo "ERROR: Windows SDK not found at '$SDK_BASE_PATH'. Please ensure the SDK is installed on your Windows host." >&2
    exit 1
fi

LATEST_SDK_VERSION=$(get-version "$SDK_BASE_PATH")
SDK_BIN_PATH="$SDK_BASE_PATH/$LATEST_SDK_VERSION/x64"
MAKEAPPX_EXE="$SDK_BIN_PATH/MakeAppx.exe"
SIGNTOOL_EXE="$SDK_BIN_PATH/SignTool.exe"

if [ ! -f "$MAKEAPPX_EXE" ] || [ ! -f "$SIGNTOOL_EXE" ]; then
    echo "ERROR: MakeAppx.exe or SignTool.exe not found in '$SDK_BIN_PATH'." >&2
    exit 1
fi
echo "    Found SDK tools in version $LATEST_SDK_VERSION."

# --- Main Script ---
echo -e "\n--- Starting MSIX build process ---"

# 1. Build the C++ project
echo "1. Building the project with CMake..."
cd "$PROJECT_ROOT"
cmake --preset mingw-release
cmake --build --preset release
echo "   Build complete."

# 2. Prepare the staging directory
echo -e "\n2. Preparing files for packaging..."
rm -rf "$STAGING_DIR"
mkdir -p "$STAGING_DIR/assets"

# Copy executable
cp "$PROJECT_ROOT/build/mingw-release/ScreenLight.exe" "$STAGING_DIR/"

# Copy required MinGW DLLs
find /usr/lib/gcc/x86_64-w64-mingw32 -name "libstdc++-6.dll" -exec cp {} "$STAGING_DIR/" \;
find /usr/lib/gcc/x86_64-w64-mingw32 -name "libgcc_s_seh-1.dll" -exec cp {} "$STAGING_DIR/" \;
find /usr/lib/gcc/x86_64-w64-mingw32 -name "libwinpthread-1.dll" -exec cp -n {} "$STAGING_DIR/" 2>/dev/null || echo "    (Info: libwinpthread-1.dll not found, continuing without it.)"

# Copy manifest and assets from the 'packaging' directory (where this script lives)
cp "$SCRIPT_DIR/AppxManifest.xml" "$STAGING_DIR/"
cp -r "$SCRIPT_DIR/src/assets"/* "$STAGING_DIR/assets/"
echo "   Staging directory prepared at '$STAGING_DIR'."

# 3. Create the MSIX package
echo -e "\n3. Creating the MSIX package..."
mkdir -p "$OUTPUT_DIR" # Create output directory if it doesn't exist

# Convert WSL paths to Windows paths for the .exe tools using wslpath
STAGING_DIR_WIN=$(wslpath -w "$STAGING_DIR")
OUTPUT_DIR_WIN=$(wslpath -w "$OUTPUT_DIR")

# Extract version from manifest to name the package dynamically
PACKAGE_VERSION=$(grep -oP '(?<=\s)Version="[^"]*"' "$STAGING_DIR/AppxManifest.xml" | sed 's/Version="\([^"]*\)"/\1/')
UNSIGNED_PACKAGE_PATH_WIN="$OUTPUT_DIR_WIN\\temp_unsigned.msix"

echo "   Creating unsigned package from '$STAGING_DIR_WIN'..."
"$MAKEAPPX_EXE" pack /d "$STAGING_DIR_WIN" /p "$UNSIGNED_PACKAGE_PATH_WIN" /o
echo "   Package created."

# 4. Sign the MSIX package (if certificate is provided)
if [ -n "$PFX_FILE" ]; then
    echo -e "\n4. Signing the package..."
    if [ ! -f "$PFX_FILE" ]; then echo "ERROR: Certificate file not found: $PFX_FILE" >&2; exit 1; fi
    PFX_FILE_WIN=$(wslpath -w "$PFX_FILE")

    # Prompt for password if PFX_PASSWORD environment variable is not set
    if [ -z "$PFX_PASSWORD" ]; then
        read -sp "Enter the password for '$PFX_FILE': " PFX_PASSWORD
        echo "" # Newline after password input
    fi

    "$SIGNTOOL_EXE" sign /f "$PFX_FILE_WIN" /p "$PFX_PASSWORD" /fd SHA256 /td SHA256 /a "$UNSIGNED_PACKAGE_PATH_WIN"
    FINAL_PACKAGE_NAME="ScreenLight_${PACKAGE_VERSION}_x64.msix"
    SUCCESS_MESSAGE="MSIX packaging complete!\nFind your signed package at:"
else
    echo -e "\n4. No certificate provided. Skipping signing."
    FINAL_PACKAGE_NAME="ScreenLight_${PACKAGE_VERSION}_x64.msix"
    SUCCESS_MESSAGE="Store-ready (unsigned) MSIX packaging complete!\nFind your package at:"
fi

# 5. Finalize and clean up
echo -e "\n5. Finalizing package..."
FINAL_PACKAGE_PATH_WSL="$OUTPUT_DIR/$FINAL_PACKAGE_NAME"
mv "$(wslpath -u "$UNSIGNED_PACKAGE_PATH_WIN")" "$FINAL_PACKAGE_PATH_WSL"

echo -e "\n--- SUCCESS ---"
echo -e "$SUCCESS_MESSAGE"
echo "$FINAL_PACKAGE_PATH_WSL"
