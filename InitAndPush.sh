#!/usr/bin/env bash
set -euo pipefail

# Usage:
#   ./InitAndPush.sh "https://github.com/USERNAME/REPO.git"
#   ./InitAndPush.sh "https://github.com/USERNAME/REPO.git" "Initial commit"
#   ./InitAndPush.sh "git@github.com:USERNAME/REPO.git" "Initial commit" "main"

REMOTE_URL="https://github.com/Liberation26/Liberation"
COMMIT_MESSAGE="${2:-Initial commit}"
BRANCH="${3:-main}"

if [[ -z "$REMOTE_URL" ]]; then
    echo "[ERROR] Missing GitHub repository URL."
    echo "Usage: $0 <repo-url> [commit-message] [branch]"
    exit 1
fi

TARGET_DIR="$(pwd)"

echo "[INFO] Working directory: $TARGET_DIR"

cd "$TARGET_DIR"

if [[ -d ".git" ]]; then
    echo "[INFO] This directory is already a Git repository."
else
    echo "[INFO] Initialising Git repository..."
    git init
fi

echo "[INFO] Setting branch to $BRANCH..."
git checkout -B "$BRANCH"

if git remote get-url origin >/dev/null 2>&1; then
    EXISTING_REMOTE="$(git remote get-url origin)"
    echo "[INFO] Remote 'origin' already exists: $EXISTING_REMOTE"

    if [[ "$EXISTING_REMOTE" != "$REMOTE_URL" ]]; then
        echo "[INFO] Updating remote 'origin' to: $REMOTE_URL"
        git remote set-url origin "$REMOTE_URL"
    fi
else
    echo "[INFO] Adding remote 'origin': $REMOTE_URL"
    git remote add origin "$REMOTE_URL"
fi

echo "[INFO] Staging files..."
git add -A

if git diff --cached --quiet; then
    echo "[INFO] No staged changes to commit."
else
    echo "[INFO] Creating commit..."
    git commit -m "$COMMIT_MESSAGE"
fi

echo "[INFO] Pushing to GitHub..."
git push -u origin "$BRANCH"

echo "[INFO] Repository created and pushed successfully."
