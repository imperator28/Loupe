#!/bin/bash
# First-run helper for the unsigned macOS build of Loupe.
#
# macOS quarantines apps downloaded from the internet and, because this build
# is not notarized, refuses to open it with a misleading "damaged" message.
# This script removes only the download-quarantine flag from Loupe.app and
# launches it. It changes nothing else on your system. Read it before running.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
app=""
for candidate in "$here/Loupe.app" "$HOME/Downloads/Loupe.app" "/Applications/Loupe.app"; do
  if [[ -d "$candidate" ]]; then app="$candidate"; break; fi
done

if [[ -z "$app" ]]; then
  echo "Could not find Loupe.app next to this script, in ~/Downloads, or in /Applications."
  printf "Drag Loupe.app into this window and press Return: "
  read -r app
  app="${app%\'}"; app="${app#\'}"   # strip quotes Finder may add
fi

if [[ ! -d "$app" ]]; then
  echo "No Loupe.app found at: $app"
  exit 1
fi

echo "Unblocking: $app"
xattr -dr com.apple.quarantine "$app"
open "$app"
echo "Done — Loupe is launching. You can close this window."
