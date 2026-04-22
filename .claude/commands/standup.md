Generate a daily standup summary from recent git activity.

Get recent commits:

```bash
git log --since="yesterday 9am" --all --oneline --author="$(git config user.name)"
```

Get open PRs:

```bash
gh pr list --author="@me" --json title,url,state,isDraft,createdAt
```

Format the output as:

**Yesterday**
- Bullet list of commits translated from technical language into plain English

**Today**
- Inferred next steps based on open PRs, in-progress branches, and recent commit themes

**Blockers**
- Open PRs that have been waiting for review more than 2 business days
- Any failing CI that is blocking progress

Keep it concise — standup should take under 2 minutes to read aloud.
