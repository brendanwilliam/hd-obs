---
name: branch-work-audit
description: Find active remote OBS-plugin branches that have not been merged into develop. Use when auditing recent contributions, identifying work to integrate, or reporting branches with unique commits from the past week.
---

# Audit Branch Work

1. Preserve the worktree and fetch the remote state:

   ```sh
   git status --short
   git fetch origin --prune
   ```

   Stop if the worktree has changes that the audit could disturb. Inspect remote-tracking branches; they represent the shared contribution state.

2. Exclude `origin/HEAD`, `origin/develop`, and `origin/main`. For every other `origin/*` branch, ignore it when `git merge-base --is-ancestor <branch> origin/develop` succeeds.

3. For each remaining branch, inspect only commits unique to it and newer than seven days:

   ```sh
   git log --format='%h %cs %an %s' --since='7 days ago' origin/develop..<branch>
   ```

   Report a branch only when this command returns a commit. Include its name, tip SHA/date, and the matching commits. Say explicitly when no branches qualify.

4. Treat the report as discovery, not approval to merge. Link each candidate to its pull request when one exists, check whether it is superseded or dependent on another branch, and recommend a review or a dedicated integration branch. Never push directly to `develop` or `main`.

Use commit reachability rather than branch age: an old branch with new unmerged work is still relevant, and a recently updated branch already reachable from `develop` is not.
