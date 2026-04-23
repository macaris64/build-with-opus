# MCP Server Setup

> Entry point: [../README.md](../README.md). Quickstart: [quickstart.md](quickstart.md). Fork/bootstrap for a new project: [fork-bootstrap.md](fork-bootstrap.md).

Operational guide for the three MCP (Model Context Protocol) servers configured in [`../../.mcp.json`](../../.mcp.json). Claude Code prompts for per-scope approval the first time each server is used in a session; this doc explains what each server provides, what it needs, and the security posture around it.

MCP is off by default. If you do not opt in to a server, Claude Code will not reach it. No server runs unless you approve it.

## Declared servers

From [`../../.mcp.json`](../../.mcp.json):

| Server | Transport | Purpose | Needs |
|---|---|---|---|
| `github` | HTTP | GitHub API access — issues, PRs, code search, branches | GitHub account + browser-based OAuth |
| `sentry` | HTTP | Flight-software anomaly tracking & performance monitoring | Sentry account (team + project membership) |
| `postgres` | stdio (`npx @bytebase/dbhub`) | Direct telemetry-database queries | Local Node.js + `DATABASE_URL` in `.env` |

## 1. `github`

HTTP transport, served by `api.githubcopilot.com/mcp/`. First approval triggers an OAuth flow in the browser.

### What it enables

- Listing and reading issues / PRs in any repo you have access to.
- Searching code across orgs.
- Posting comments, creating branches, opening PRs (when you approve write actions).

### Setup

1. Start Claude Code inside this repo (`cd build-with-opus && claude`).
2. On first use, Claude will ask to approve the `github` server. Approve **at the scope you want** — project-scope (this repo only) is the default.
3. The OAuth window opens in your default browser. Complete sign-in.
4. Verify: ask Claude to run a tool like `list open issues in this repo`.

### Security notes

- OAuth tokens live in your Claude Code session state, not in `.mcp.json`.
- Write actions (opening PRs, posting comments) still prompt for per-tool approval even after the server is approved.
- Revoke access any time from `https://github.com/settings/applications`.

## 2. `sentry`

HTTP transport, served by `mcp.sentry.dev/mcp`. OAuth-based like `github`.

### What it enables

- Query recent issues, grouped events, release health.
- Follow stack traces and breadcrumbs into source lines.
- Cross-reference flight-software anomalies with Sentry-tracked runtime errors on the ground-station side.

### Setup

Same flow as `github` — approve on first use, complete browser OAuth. Your Sentry team + project membership determines what the server can see; no extra configuration in this repo.

### What the project expects to see in Sentry

- Ground-station Rust panics (via `sentry`-crate integration — not yet wired, planned under the operator-UI work per [`../architecture/06-ground-segment-rust.md §10`](../architecture/06-ground-segment-rust.md)).
- Future: event-log entries from `CFE_EVS` bubbled up via a ground-side shim (deferred).

## 3. `postgres`

stdio transport. The config runs `npx -y @bytebase/dbhub` against a DSN from the `DATABASE_URL` environment variable (defaults to `postgresql://localhost:5432/dev` if unset).

### Prerequisites

- **Node.js** (LTS). The `npx` invocation pulls `@bytebase/dbhub` on first run.
- A Postgres instance reachable at `DATABASE_URL`. No project-provided instance today — use your own local dev DB.

### Setup

1. Create `.env` in the repo root (it is gitignored):

   ```bash
   echo 'DATABASE_URL=postgres://user:pass@localhost:5432/telemetry' > .env
   ```

   Credentials must come from your own development account — **never** commit credentials.

2. Start Claude Code. Approve the `postgres` server on first use.

3. `npx` fetches `@bytebase/dbhub` (a few MB). Subsequent sessions reuse the cache.

4. Verify: ask Claude to `list tables in the current database`.

### Security notes

- `DATABASE_URL` is read into the Node child process's environment at server start. Anyone who can read your process table can see the DSN — **do not put a production DSN here**.
- The [`protect-secrets.sh`](../../.claude/hooks/protect-secrets.sh) hook refuses any write to `.env` from within Claude Code. You set `DATABASE_URL` yourself; Claude Code will not.
- `cargo audit` and similar scans do **not** cover Node dependencies. If you are concerned about supply-chain risk in `@bytebase/dbhub`, pin the version in `.mcp.json` or vendor the package.

### Not using a DB today?

That's fine — deny the server on first prompt, or remove its entry from `.mcp.json` in your local fork. The project does not require it for builds or tests; it exists as a demonstration of the stdio MCP pattern for telemetry-adjacent tooling.

## Approving servers

Claude Code asks once per scope. You have three options:

| Scope | Effect |
|---|---|
| **This session only** | Approval expires when you close Claude Code |
| **This project** | Persists in `.claude/settings.local.json` (gitignored) |
| **Globally** | Persists in `~/.claude/settings.json` |

Project-scope is the usual choice for this repo — you want the same approvals next time you work on SAKURA-II, but you probably don't want the same Postgres DSN to apply to unrelated projects.

## Revoking or disabling

- **Temporarily deny**: close Claude Code; the server will re-prompt next session.
- **Permanently deny in this repo**: add an entry under `mcpServers` in [`.claude/settings.local.json`](../../.claude/settings.local.json.example) with `"enabled": false` (schema per Claude Code docs).
- **Remove for everyone**: delete the server from [`../../.mcp.json`](../../.mcp.json) and commit — this is a team-visible change.

## Where to read more

- Claude Code MCP documentation (upstream).
- [`../../CLAUDE.md`](../../CLAUDE.md) § MCP Servers — the short overview embedded in project instructions.
- [`.claude/settings.json`](../../.claude/settings.json) permission-model details.
