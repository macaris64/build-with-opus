---
argument-hint: "[issue-number]"
---

Fix GitHub issue #$ARGUMENTS.

Read the issue with `gh issue view $ARGUMENTS`, understand the requirements, explore the relevant code, implement the fix following the conventions in CLAUDE.md, write a test that would have caught the bug, then verify:

- C/cFS changes: `cmake --build build && ctest --test-dir build --output-on-failure && cppcheck --enable=all apps/`
- Rust changes: `cargo test && cargo clippy -- -D warnings`
- ROS 2 changes: `cd ros2_ws && colcon build && colcon test`

Then commit: `git commit -m "fix: resolve #$ARGUMENTS - <one-line description>"`.
