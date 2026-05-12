# Handoff Protocol

Use this protocol when multiple AI sessions work on this project over time.

## Start Of Session

Use the low-token path by default. Do not ask the user to remind you.

1. Read `docs/PROJECT_INDEX.md`.
2. Read only the top 1-3 entries in `docs/SESSION_LOG.md`.
3. Inspect only files related to the requested task.

Use the full `docs/AI_HANDOFF.md` only when the task touches architecture, platform channels, native rendering, Cesium Native integration, or when the short docs are not enough.

Quick shell pattern:

```sh
sed -n '1,160p' docs/PROJECT_INDEX.md
sed -n '1,120p' docs/SESSION_LOG.md
rg --files lib android/app/src/main/kotlin android/app/src/main/AndroidManifest.xml android/app/build.gradle.kts docs
```

This directory is currently not guaranteed to be a git repository, so do not depend on git history existing. Treat the files on disk as the source of truth.

## During Work

- In early PoC tasks, take large practical steps toward a runnable end-to-end path. Do not get stuck polishing minor edge cases before the main capability works.
- Keep changes small and easy to verify.
- Keep code lean and modular. Avoid adding broad utility layers, generic frameworks, or speculative abstractions.
- Split files only around real responsibilities: Flutter UI, platform channel, native view lifecycle, tile loading/cache, renderer, Cesium Native bridge, or platform-specific integration.
- Before adding a new dependency or module, check whether a small local function or existing file is enough.
- Do not add automated test code during the early PoC phase. Prefer `flutter analyze`, APK build, and emulator/device validation records.
- Preserve the Flutter/native channel contract unless the task explicitly changes it.
- Do not overwrite or revert unknown changes from another session.
- If code differs from `docs/AI_HANDOFF.md`, trust the code first, then update the docs.
- Prefer adding a short log entry over trying to make the handoff document perfect every time.
- Keep handoff docs concise. Do not paste large command outputs, full files, screenshots, or stack traces into docs; summarize and point to file paths when needed.

## End Of Session

Before finishing a meaningful code or documentation change:

1. Run the strongest reasonable verification for the change.
2. Update `docs/SESSION_LOG.md`.
3. Mention any command that failed or was skipped.
4. Record unresolved risks or next steps.

Minimum verification for normal code changes:

```sh
flutter analyze
flutter build apk --debug --target-platform android-arm64
```

If an Android device is available, also install and launch the APK, then capture a screenshot or record the observed result.

## Session Log Entry Format

Append entries at the top of `docs/SESSION_LOG.md`:

```md
## YYYY-MM-DD HH:mm Local - Short Title

- Changed:
- Verified:
- Not verified:
- Notes / risks:
```

Keep entries concise. The goal is continuity, not ceremony.
