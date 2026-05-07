---
phase: 1
slug: distribution
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-05-07
---

# Phase 1 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Manual smoke tests only — no automated test framework applicable |
| **Config file** | none — no new test files needed |
| **Quick run command** | `ninja -C build -j12` (build verification) |
| **Full suite command** | `ninja -C build cereka_test && ./build/tests/cereka_test` |
| **Estimated runtime** | ~30 seconds (build) |

---

## Sampling Rate

- **After every task commit:** Run `ninja -C build -j12`
- **After every plan wave:** Run `ninja -C build cereka_test && ./build/tests/cereka_test`
- **Before `/gsd-verify-work`:** Full suite must be green
- **Max feedback latency:** 30 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 1-01-01 | SDL3 static Windows | 1 | — | — | Static binary eliminates DLL hijacking surface | manual | `dumpbin /dependents CerekaGame.exe` (Windows) | ✅ | ⬜ pending |
| 1-01-02 | Linux RPATH | 1 | — | — | `$ORIGIN` RPATH prevents LD_LIBRARY_PATH hijacking | manual | `ldd CerekaGame` (Linux) | ✅ | ⬜ pending |
| 1-01-03 | SDL .so routing | 1 | — | — | N/A | build | `ninja -C build -j12` | ✅ | ⬜ pending |
| 1-02-01 | Launcher packager | 2 | — | — | N/A | manual | Run doPackage() on both platforms | ✅ | ⬜ pending |
| 1-03-01 | windeployqt | 2 | — | — | N/A | manual | Run on fresh Windows VM | ✅ | ⬜ pending |
| 1-04-01 | AppImage | 2 | — | — | N/A | manual | `./CerekaLauncher.AppImage` on Ubuntu LTS | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

Existing infrastructure covers all phase requirements. No new test files needed — this phase has no unit-testable logic (entirely build system and packaging configuration).

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Windows CerekaGame.exe has zero DLL dependencies | Windows player distribution | Cannot run Windows tools on macOS dev machine | Run on fresh Windows VM: `dumpbin /dependents CerekaGame.exe`; expect only kernel32/ntdll/user32 etc. |
| Linux CerekaGame finds SDL `.so` from its own dir | Linux player distribution | Requires Linux runtime | Move binary+.so to temp dir, run `ldd ./CerekaGame` — all SDL entries should resolve to local dir |
| Linux game package ZIP extracts and runs | Linux player distribution | Requires Linux runtime | Unzip into temp dir, run `./CerekaGame` — must launch without system SDL |
| Windows launcher runs on fresh machine | Windows dev tool distribution | Requires fresh Windows VM | Test on VM without Qt6: launch `CerekaLauncher.exe` — must open without DLL errors |
| AppImage runs on stock Ubuntu LTS | Linux dev tool distribution | Requires Linux + specific tooling | `chmod +x CerekaLauncher.AppImage && ./CerekaLauncher.AppImage` on Ubuntu 22.04 |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 30s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
