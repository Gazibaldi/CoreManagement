# 🎯 Technical Assessment and Modernization Findings

Create a comprehensive, evidence-based technical assessment of the SoftCores solution and the sibling `../CoreSDK`, saved as a multi-file Markdown report under the solution-root `plans/findings/` directory. The report is documentation only: it should characterize the current implementation, dependency architecture, runtime behavior, build/tooling posture, defects and risks, maintainability, testability, security/reliability concerns, and modernization opportunities without changing production code.

Verified starting context:
- `main.cpp` is a Windows DLL entry point that registers `ScriptMain` and `OnKeyboardMessage` through CoreSDK/ScriptHook imports.
- `script.cpp` currently centralizes configuration, logging, gameplay state detection, sleep/bath behavior, player and horse core management, timers, and the perpetual `WAIT(0)` loop.
- `script.h` directly consumes `../CoreSDK/inc/{natives,types,enums,main}.h`.
- `keyboard.cpp` maintains process-global key state using `GetTickCount()` and fixed timing windows.
- `../CoreSDK/inc/main.h` defines imported ScriptHook lifecycle/native/world APIs, `types.h` defines ABI-sensitive aliases and an aligned `Vector3`, and `natives.h` is a large generated NativeDB wrapper using `nativeCaller.h`.
- Concrete issues already visible include uninitialized loop-local state in sleep/bath handling, questionable time-of-day coverage, timer rollover exposure, repeated native lookups, broad responsibility concentration, fragile horse/entity assumptions, global mutable logger state, and absent test seams.
- README documentation is minimal, and initial reads did not resolve project/solution metadata, a checked-in runtime INI, or tests. The final report must update these statements after a broader inventory and clearly label anything that remains unavailable or unverified.

Use a navigable report set, recommended as:
- `plans/findings/README.md` — index, scope, methodology, executive summary, report links, and assessment limitations.
- `plans/findings/01-solution-inventory.md` — repository/project structure, build configuration, source inventory, external/runtime assets, and documentation state.
- `plans/findings/02-softcores-architecture.md` — lifecycle, modules, runtime flow, state/configuration/logging, feature behavior, coupling, and diagrams where useful.
- `plans/findings/03-coresdk-assessment.md` — full CoreSDK structure, generated versus hand-maintained code, native invocation mechanism, ABI boundaries, ScriptHook linkage, version compatibility, dependency graph, and maintenance/update process.
- `plans/findings/04-quality-risks.md` — evidence-backed defects and risks categorized by correctness, reliability, performance, security, compatibility, maintainability, and operability, each with severity, impact, evidence, and remediation direction.
- `plans/findings/05-testing-and-tooling.md` — current build/test/static-analysis/toolchain posture, testability gaps, and proposed verification strategy.
- `plans/findings/06-modernization-roadmap.md` — prioritized, dependency-aware modernization/refactor stages that preserve game behavior while preparing for feature extensions.

Keep findings factual and traceable with file/line or symbol references. Separate confirmed defects from suspected risks and unknowns. Do not copy large generated or third-party source blocks into the reports; summarize interfaces and provenance. Call out licensing/provenance gaps without asserting legal conclusions. Rank recommendations with a consistent severity/priority model and identify prerequisites, expected value, migration risk, and validation approach. The roadmap should recommend direction, not prematurely lock in feature requirements that have not yet been supplied.

**Last Updated**: 2026-10-04 02:57:50

## 📝 Plan Steps
-  **Inventory the solution root and `../CoreSDK` — identify solution/project files, source and header trees, generated artifacts, libraries, configuration samples, documentation, build outputs, tests, CI files, and ownership/provenance indicators; record anything inaccessible as an explicit assessment limitation.**
-  **Analyze build and toolchain configuration — document Visual Studio platform/toolset, language standard, warning and optimization settings, output/link behavior, ScriptHook/CoreSDK references, architecture assumptions, deployment expectations, and reproducibility gaps.**
-  **Trace SoftCores runtime architecture — map DLL attach/detach, script and keyboard registration, `ScriptMain`, the frame loop, native API calls, configuration loading, logging, timers, state transitions, and player/horse feature behavior.**
-  **Assess every SoftCores source module — review correctness, lifetime/state management, API usage, entity validation, timing arithmetic, error handling, performance, naming/style, coupling, cohesion, portability, and testability with precise evidence references.**
-  **Perform a full CoreSDK assessment — inspect all CoreSDK projects and files, classify generated and maintained code, trace `nativeCaller` invocation and ABI/type handling, review import/library integration and game-version support, assess generated NativeDB freshness and regeneration workflow, and map all services exposed to SoftCores.**
-  **Map dependencies and compatibility boundaries — distinguish Microsoft/Windows APIs, ScriptHook SDK components, alloc8or NativeDB output, game-native contracts, local code, and runtime files; document versioning, deployment, licensing/provenance, and upgrade risks.**
-  **Evaluate quality infrastructure — locate or confirm the absence of tests, mocks/fakes, static analysis, sanitizers, formatting/linting, package/dependency management, CI, release automation, diagnostics, and reproducible setup documentation.**
-  **Build a prioritized findings register — assign stable finding IDs, severity and confidence, describe impact and triggering conditions, cite source evidence, distinguish confirmed issues from hypotheses, and suggest focused remediation and verification methods.**
-  **Design the modernization roadmap — sequence safety fixes, characterization/testing seams, modular decomposition, configuration and logging modernization, CoreSDK isolation, type-safe native adapters, tooling/build upgrades, and later feature-extension readiness with prerequisites and risk controls.**
-  **Write the multi-file report under `plans/findings/` — create the index and detailed documents with consistent terminology, cross-links, tables, concise diagrams, and a clear split between current-state evidence and future recommendations.**
-  **Validate the assessment package — verify links and references, ensure both SoftCores and CoreSDK are fully represented, reconcile duplicate or conflicting findings, confirm no production files were changed, and add a final list of unanswered questions and evidence that requires runtime or game-level validation.**

