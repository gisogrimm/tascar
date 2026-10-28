#!/bin/bash

# Define the repository owner and name
REPO_OWNER="gisogrimm"
REPO_NAME="tascar"

# Define a temporary directory for downloads and extraction
TEMP_DIR="/tmp/tascar_install"
mkdir -p "$TEMP_DIR" || { echo "Error: Failed to create temporary directory"; exit 1; }

# Function to handle errors
handle_error() {
    echo "Error: $1"
    echo "Cleaning up temporary files..."
    rm -rf "$TEMP_DIR" 2>/dev/null
    exit 1
}

# Function to check if a command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Check for required commands
for cmd in curl wget unzip apt lsb_release dpkg; do
    if ! command_exists "$cmd"; then
        handle_error "Required command '$cmd' is not installed"
    fi
done

# Function to get architecture in the format expected by GitHub releases
get_github_arch() {
    local arch=$(dpkg --print-architecture)
    case "$arch" in
        amd64)
            echo "x64"
            ;;
        i386|i686)
            echo "x86"
            ;;
        arm64|aarch64)
            echo "arm64"
            ;;
        armhf|armv7l)
            echo "arm"
            ;;
        *)
            echo "$arch"  # Fallback to whatever dpkg returns
            ;;
    esac
}

# Get system information
echo "Detecting system information..."
DISTRO_ID=$(lsb_release -si | tr '[:upper:]' '[:lower:]')
DISTRO_VERSION=$(lsb_release -sr | tr '[:upper:]' '[:lower:]')
GITHUB_ARCH=$(get_github_arch)

echo "Detected: $DISTRO_ID $DISTRO_VERSION ($GITHUB_ARCH)"

# Get the latest release tag using GitHub API
echo "Fetching latest release information..."
LATEST_RELEASE_TAG=$(curl -s "https://api.github.com/repos/$REPO_OWNER/$REPO_NAME/releases/latest" | \
                    grep '"tag_name":' | \
                    sed -E 's/.*"([^"]+)".*/\1/' | \
                    head -n 1) || handle_error "Failed to fetch latest release tag"

if [ -z "$LATEST_RELEASE_TAG" ]; then
    handle_error "No release tag found in GitHub API response"
fi

# Extract release number from tag
LATEST_RELEASE_NUMBER=$(echo "${LATEST_RELEASE_TAG}" | sed -e 's/release_//1')

# Define the URL of the latest release using detected system info
LATEST_RELEASE_URL="https://github.com/$REPO_OWNER/$REPO_NAME/releases/download/$LATEST_RELEASE_TAG/tascar-$LATEST_RELEASE_NUMBER-$DISTRO_ID-$DISTRO_VERSION-$GITHUB_ARCH.zip"

echo "Latest release: $LATEST_RELEASE_TAG"
echo "Downloading from: $LATEST_RELEASE_URL"

# Download the latest release with progress
echo "Downloading release..."
wget --progress=dot:giga "$LATEST_RELEASE_URL" -O "$TEMP_DIR/tascar-latest.zip" || handle_error "Failed to download release"

# Verify the downloaded file
if [ ! -s "$TEMP_DIR/tascar-latest.zip" ]; then
    handle_error "Downloaded file is empty or doesn't exist"
fi

# Extract the contents
echo "Extracting files..."
unzip -q "$TEMP_DIR/tascar-latest.zip" -d "$TEMP_DIR" || handle_error "Failed to extract zip file"

# Find all .deb files recursively
mapfile -t DEB_FILES < <(find "$TEMP_DIR" -name "*.deb" -type f)

if [ ${#DEB_FILES[@]} -eq 0 ]; then
    handle_error "No .deb files found in the extracted archive"
fi

# Get unique directories containing .deb files
mapfile -t DEB_DIRS < <(find "$TEMP_DIR" -name "*.deb" -type f -exec dirname {} \; | sort -u)

echo "Found ${#DEB_FILES[@]} .deb files in ${#DEB_DIRS[@]} directories"

# Install all .deb files
echo "Installing .deb packages..."
sudo apt update || handle_error "Failed to update package lists"

for deb_dir in "${DEB_DIRS[@]}"; do
    echo "Installing packages from directory: $deb_dir"
    (
        cd "$deb_dir" || handle_error "Failed to enter directory $deb_dir"
        sudo apt install -y ./*.deb || handle_error "Failed to install packages from $deb_dir"
    )|| handle_error "Installation failed."
done

# Verify installation
echo "Verifying installation..."
if ! command_exists "tascar"; then
    echo "Warning: 'tascar' command not found in PATH after installation"
fi

# Clean up
echo "Cleaning up temporary files..."
rm -rf "$TEMP_DIR" && echo "Temporary files removed successfully" || echo "Warning: Failed to clean up temporary files"

echo "Installation completed successfully. Latest release installed: $LATEST_RELEASE_TAG"
