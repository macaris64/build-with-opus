#!/usr/bin/env bash
# Called when Claude finishes a turn. Sends a desktop notification.
# Runs async (non-blocking) — never delays Claude's response.

set -euo pipefail

MESSAGE="Claude Code finished"

# macOS
if command -v osascript &>/dev/null; then
  osascript -e "display notification \"$MESSAGE\" with title \"Claude Code\"" 2>/dev/null || true
  exit 0
fi

# Linux — libnotify
if command -v notify-send &>/dev/null; then
  notify-send "Claude Code" "$MESSAGE" 2>/dev/null || true
  exit 0
fi

# Linux — dunst
if command -v dunstify &>/dev/null; then
  dunstify "Claude Code" "$MESSAGE" 2>/dev/null || true
  exit 0
fi

# Windows (WSL) — PowerShell balloon tip
if command -v powershell.exe &>/dev/null; then
  powershell.exe -Command "
    Add-Type -AssemblyName System.Windows.Forms
    \$n = New-Object System.Windows.Forms.NotifyIcon
    \$n.Icon = [System.Drawing.SystemIcons]::Information
    \$n.Visible = \$true
    \$n.ShowBalloonTip(3000, 'Claude Code', '$MESSAGE', [System.Windows.Forms.ToolTipIcon]::Info)
  " 2>/dev/null || true
  exit 0
fi

exit 0
