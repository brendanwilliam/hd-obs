---
name: obs-github-issues
description: Discover an explicitly selected assigned OBS GitHub Issue and start governed issue-backed work. Use when listing assigned OBS issues, selecting an OBS issue, or creating its branch and pull request plan.
---

# OBS GitHub Issues

1. Confirm this is the `hd-obs` repository, then run `gh auth status`. Stop and ask the user to authenticate if it fails; do not use another identity or repository.
2. List only open issues assigned to the authenticated current user in this repository, for example with `gh issue list --assignee @me --state open`. Do not select an issue on the user's behalf; wait for an explicit issue number or URL.
3. Fetch the selected issue's full title, body, labels, and comments. Reconcile it with the user's request and ask about any material conflict before changing code.
4. Fetch `origin`, inspect the worktree, and create a branch from `origin/develop` named exactly one of `feature/<issue>-<slug>`, `fix/<issue>-<slug>`, or `chore/<issue>-<slug>`. Choose the type from the agreed work.
5. Target `develop`. For the same-repository issue-backed pull request, include `Closes #<issue>` in its body.
6. Never automatically assign, label, close, edit, or otherwise mutate the GitHub Issue. Preserve the plugin's privacy and Accessibility safeguards and follow `start-change` and `prepare-pr` for implementation and validation.
