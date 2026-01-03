#!/bin/bash
set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
AUR_DIR="aur"
MAIN_BRANCH="main"

# Helper functions
print_header() {
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}  Update Notifier Qt Release Script${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo
}

print_step() {
    echo -e "${GREEN}➤ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠️  $1${NC}"
}

print_error() {
    echo -e "${RED}❌ $1${NC}"
}

print_success() {
    echo -e "${GREEN}✅ $1${NC}"
}

# Validate version format (semantic versioning)
validate_version() {
    local version=$1

    # Remove 'v' prefix if present for validation
    local clean_version=${version#v}

    # Check semantic version format (major.minor.patch)
    if ! [[ $clean_version =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
        print_error "Invalid version format: $version"
        echo "Expected format: 1.0.0 or v1.0.0"
        exit 1
    fi

    echo "$version"
}

# Check if tag already exists
check_tag_exists() {
    local version=$1

    # Check local tags
    if git tag -l | grep -q "^${version}$"; then
        print_error "Tag '$version' already exists locally"
        return 1
    fi

    # Check remote tags
    if git ls-remote --tags origin 2>/dev/null | grep -q "refs/tags/${version}$"; then
        print_error "Tag '$version' already exists on remote"
        return 1
    fi

    return 0
}

# Get latest tag version for comparison
get_latest_tag() {
    local latest_tag=$(git tag -l | sort -V | tail -n1)
    if [ -z "$latest_tag" ]; then
        echo "0.0.0"  # No tags exist yet
    else
        echo "${latest_tag#v}"  # Remove 'v' prefix for comparison
    fi
}

# Compare versions (returns 0 if new > old, 1 if new <= old)
compare_versions() {
    local new_version=$1
    local old_version=$2

    # Remove 'v' prefix for comparison
    new_version=${new_version#v}
    old_version=${old_version#v}

    # Use sort -V for semantic version comparison
    if [ "$new_version" = "$(echo -e "$new_version\n$old_version" | sort -V | tail -n1)" ] && [ "$new_version" != "$old_version" ]; then
        return 0  # new > old
    else
        return 1  # new <= old
    fi
}

# Prompt user for annotation
prompt_annotation() {
    local version=$1

    echo
    print_step "Enter release annotation/notes (press Ctrl+D when done):"
    echo "Suggested format:"
    echo "## Release $version"
    echo
    echo "- Feature 1"
    echo "- Bug fix 2"
    echo "- Other changes"
    echo

    # Read multi-line input
    local annotation=""
    while IFS= read -r line; do
        annotation="${annotation}${line}\n"
    done

    # Remove trailing newline
    annotation=$(echo -e "$annotation" | sed '/^$/d')

    if [ -z "$annotation" ]; then
        print_error "Annotation cannot be empty"
        exit 1
    fi

    echo "$annotation"
}

# Create annotated tag
create_tag() {
    local version=$1
    local annotation=$2

    print_step "Creating annotated tag '$version'..."

    # Create annotated tag
    git tag -a "$version" -m "$annotation"

    print_success "Tag '$version' created successfully"
    echo
    git show "$version" --stat
}

# Update AUR package
update_aur_package() {
    local version=$1
    local annotation=$2

    print_step "Updating AUR package..."

    # Check if aur directory exists
    if [ ! -d "$AUR_DIR" ]; then
        print_error "AUR directory '$AUR_DIR' not found"
        exit 1
    fi

    # Change to aur directory
    cd "$AUR_DIR"

    # Regenerate .SRCINFO
    print_step "Regenerating .SRCINFO..."
    makepkg --printsrcinfo > .SRCINFO

    # Check if .SRCINFO changed
    if git diff --quiet .SRCINFO; then
        print_warning ".SRCINFO unchanged - no commit needed"
    else
        print_step "Committing AUR package update..."

        # Add and commit changes
        git add .SRCINFO
        git commit -m "$annotation"

        print_success "AUR package updated and committed"
        echo
        git show --stat HEAD
    fi

    # Go back to original directory
    cd ..
}

# Show manual push instructions
show_push_instructions() {
    local version=$1

    echo
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}  MANUAL PUSH REQUIRED${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo
    print_warning "Please run these commands manually:"
    echo
    echo "# Push the tag to GitHub:"
    echo -e "${YELLOW}git push origin $version${NC}"
    echo
    echo "# Push AUR package update:"
    echo -e "${YELLOW}cd aur && git push${NC}"
    echo
    print_step "After pushing, the AUR package will be updated automatically"
}

# Main script
main() {
    local version=""

    # Parse arguments
    if [ $# -eq 0 ]; then
        print_error "Usage: $0 <version>"
        echo "Example: $0 1.0.0 or $0 v1.0.0"
        exit 1
    fi

    version=$1

    print_header

    # Validate and normalize version
    print_step "Validating version format..."
    version=$(validate_version "$version")
    print_success "Version format valid: $version"

    # Check if we're in a git repository
    if ! git rev-parse --git-dir > /dev/null 2>&1; then
        print_error "Not in a git repository"
        exit 1
    fi

    # Check if on main branch
    local current_branch=$(git branch --show-current)
    if [ "$current_branch" != "$MAIN_BRANCH" ]; then
        print_warning "Not on $MAIN_BRANCH branch (currently on: $current_branch)"
        echo "Continue anyway? (y/N)"
        read -r response
        if [[ ! "$response" =~ ^[Yy]$ ]]; then
            print_step "Aborted by user"
            exit 0
        fi
    fi

    # Check if tag exists
    print_step "Checking if tag '$version' already exists..."
    if ! check_tag_exists "$version"; then
        exit 1
    fi
    print_success "Tag '$version' is available"

    # Compare with latest tag
    print_step "Checking version progression..."
    local latest_tag=$(get_latest_tag)
    local clean_version=${version#v}

    if ! compare_versions "$clean_version" "$latest_tag"; then
        print_error "Version $clean_version is not higher than latest tag $latest_tag"
        exit 1
    fi
    print_success "Version $version > $latest_tag ✓"

    # Prompt for annotation
    local annotation=$(prompt_annotation "$version")

    # Confirm before proceeding
    echo
    print_warning "About to create tag '$version' with annotation:"
    echo "$annotation"
    echo
    echo "Continue? (y/N)"
    read -r response
    if [[ ! "$response" =~ ^[Yy]$ ]]; then
        print_step "Aborted by user"
        exit 0
    fi

    # Create the tag
    create_tag "$version" "$annotation"

    # Update AUR package
    update_aur_package "$version" "$annotation"

    # Show manual push instructions
    show_push_instructions "$version"

    echo
    print_success "Release preparation complete!"
    print_step "Don't forget to push the changes manually"
}

# Run main function with all arguments
main "$@"