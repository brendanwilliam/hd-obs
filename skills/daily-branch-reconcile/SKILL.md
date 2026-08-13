---
name: daily-branch-reconcile
description: Consolidate unmerged OBS-plugin commits from non-develop and non-main remote branches made in the past 24 hours. Use when creating a daily integration branch, adding all recent branch work, or resolving its conflicts.
---

# Reconcile Daily Branches

1. Confirm the worktree is clean, then fetch the shared branch state:

   ```sh
   git status --short
   git fetch origin --prune
   ```

   Stop on unrelated worktree changes. Do not use local-only branches as inputs unless the requester explicitly includes them.

2. Collect the exact unique commits reachable from `origin/*` branches, excluding commits reachable from `origin/develop` or `origin/main`, with a commit date in the last 24 hours:

   ```sh
   git rev-list --topo-order --reverse --since='24 hours ago' \
     --remotes=origin --not origin/develop origin/main
   ```

   Deduplicate the SHAs, inspect each with `git show --stat --oneline`, and report the source branches using `git branch -r --contains <sha>`. Say explicitly when there are no eligible commits.

3. Create a new branch from current `origin/develop` named `chore/daily-branch-reconcile-YYYY-MM-DD`. If that name already exists, use an unused numeric suffix; do not reuse or overwrite an existing integration branch.

4. Cherry-pick the selected SHAs in the discovered topological order. This intentionally integrates only the past day's commits rather than merging older source-branch history. Preserve authorship and record the source branches in the final handoff.

5. On a conflict, inspect both sides and resolve the intended combined behavior. Stage the resolution and run `git cherry-pick --continue`. When Git reports an empty pick because the same change is already present, verify that fact and run `git cherry-pick --skip`. Do not discard the integration branch or force-push it.

6. Run the CI-equivalent configuration and build, then push the new branch and target a pull request at `develop`. Never push directly to `develop` or `main`.
