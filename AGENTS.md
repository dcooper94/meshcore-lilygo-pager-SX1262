# Development workflow for this repo

## Branch strategy

- `main` — stable, released code only. Never commit directly.
- `feature/<short-slug>` — new features (e.g. `feature/reply-and-path-info`)
- `fix/<short-slug>` — bug fixes (e.g. `fix/backlight-override`)
- `chore/<short-slug>` — non-code changes: docs, deps, CI (e.g. `chore/update-readme`)

## Making a change

1. **Open an issue** before starting work:
   ```bash
   gh issue create --repo dcooper94/meshcore-lilygo-pager-SX1262 \
     --title "..." \
     --label "bug"        # or "enhancement" for features
     --body "..."
   ```
   One issue per branch. Note the issue number — you'll need it for the PR.

2. **Create a branch** from `main`, named after the issue:
   ```bash
   git checkout main && git pull personal main
   git checkout -b feature/<slug>   # or fix/<slug>
   ```

3. **Make commits** on the branch. Commit messages:
   - Imperative present tense: "Fix backlight override on keypress"
   - One short subject line (≤ 72 chars)
   - Optional body explaining *why*, not *what*
   - Always end with:
     ```
     Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
     ```
4. **Push** and open a PR against `main`:
   ```bash
   git push personal feature/<slug>
   gh pr create --repo dcooper94/meshcore-lilygo-pager-SX1262 \
     --base main --head feature/<slug> \
     --title "..." --body "..."
   ```
   PR body must include:
   - **What**: one-paragraph description of the change
   - **Why**: motivation / user-facing impact
   - **Test**: what was flashed and verified on hardware
   - A `Closes #<issue-number>` line so GitHub auto-closes the issue on merge
   - Attribution line: `🤖 Generated with [Claude Code](https://claude.com/claude-code)`

5. **Update the PR description** whenever new commits are pushed to the branch:
   ```bash
   gh pr edit <number> --repo dcooper94/meshcore-lilygo-pager-SX1262 \
     --body "..."
   ```
   The **Test** section must reflect what was actually flashed and verified, not
   the original test plan. Add an **Updates** section listing what changed since
   the PR was opened if the scope shifted materially.

6. **Merge** the PR on GitHub (squash or merge commit, your choice). GitHub will
   auto-close the linked issue.

7. **Release** — optional, can batch multiple merged PRs. Cut one when there
   is at least one user-visible change worth shipping. Chore/doc-only PRs do
   not need their own release; batch them with the next feature or fix.
   Before releasing, run `clang-format --style=file -i` on any changed
   `*.cpp`/`*.h` files in `variants/lilygo_tlora_pager_sx1262/` (excluding
   `lv_conf.h`) so the format-check passes on the release commit.
   ```bash
   git checkout main && git pull personal main
   pio run -e T_LoRa_Pager_SX1262_companion_radio_usb   # build release firmware
   # package zip (see What lives where)
   dd if=.pio/build/T_LoRa_Pager_SX1262_companion_radio_usb/firmware.bin \
      of=/tmp/tpager-firmware/tpager-sx1262-vX.Y.Z.bin
   cp /tmp/tpager-firmware/tpager-sx1262-vX.Y.Z.bin /tmp/tpager-firmware/firmware.bin
   cd /tmp/tpager-firmware && zip -j /tmp/tpager-sx1262-firmware.zip *.bin && cd -
   git tag vX.Y.Z
   git push personal vX.Y.Z
   gh release create vX.Y.Z \
     --repo dcooper94/meshcore-lilygo-pager-SX1262 \
     --title "vX.Y.Z — <short description>" \
     --notes "..." \
     /tmp/tpager-sx1262-firmware.zip
   ```

## Versioning

`vMAJOR.MINOR.PATCH` — increment:
- MAJOR: breaking change to flash layout or NVS schema (requires full reflash)
- MINOR: new user-visible feature
- PATCH: bug fix, docs, or power/performance improvement

Current version: **v1.2.1**

## Build and flash

```bash
# Build
pio run -e T_LoRa_Pager_SX1262_companion_radio_usb

# Split for two-part flash (avoids esptool no-stub timeout)
dd if=.pio/build/T_LoRa_Pager_SX1262_companion_radio_usb/firmware.bin \
   of=/tmp/fw_part1.bin bs=1 count=524288
dd if=.pio/build/T_LoRa_Pager_SX1262_companion_radio_usb/firmware.bin \
   of=/tmp/fw_part2.bin bs=1 skip=524288

# Flash part 1 (hold BOOT, tap RST, release BOOT first)
python -m esptool --chip esp32s3 --port /dev/ttyACM0 --baud 115200 \
  --no-stub --before default_reset --after no_reset \
  write_flash --flash_mode dio --flash_freq 80m --flash_size 16MB \
  0x10000 /tmp/fw_part1.bin

# Flash part 2 (device stays in bootloader after part 1)
python -m esptool --chip esp32s3 --port /dev/ttyACM0 --baud 115200 \
  --no-stub --before default_reset --after hard_reset \
  write_flash --flash_mode dio --flash_freq 80m --flash_size 16MB \
  0x90000 /tmp/fw_part2.bin
```

## Code style

- `variants/lilygo_tlora_pager_sx1262/` files are clang-format checked on every PR.
  Run before pushing: `clang-format --style=file -i <changed files>` (skip `lv_conf.h`).
- Variant files (`variants/lilygo_tlora_pager_sx1262/`) use 2-space indent
- No comments explaining *what* code does — only *why* when non-obvious
- No trailing summaries in commit messages

## What lives where

| Path | Purpose |
|------|---------|
| `variants/lilygo_tlora_pager_sx1262/` | All SX1262-specific code |
| `examples/companion_radio/` | Shared MeshCore app (upstream — minimise changes) |
| `/tmp/tpager-firmware/` | Staging dir for release zip |
| `/tmp/tpager-sx1262-firmware.zip` | Distribution zip (update after every release build) |

## Upstream sync

This repo is a fork of [CJvanSoest/meshcore-lilygo-pager-LR1121](https://github.com/CJvanSoest/meshcore-lilygo-pager-LR1121).
Upstream changes are merged periodically into `main` via `git fetch origin && git merge origin/main`.
Avoid editing shared files outside `variants/lilygo_tlora_pager_sx1262/` unless absolutely necessary; prefer weak bridge functions so upstream merges stay clean.
