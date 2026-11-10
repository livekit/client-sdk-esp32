# verwalter adoption summary

This file records how this repository moved from manual releases to
[verwalter](https://github.com/livekit/verwalter) (v0.29.0). It lists the
decisions we made, the limitations we found, and the steps a maintainer
must do. Remove this file when the open items are closed.

## What changed

- `verwalter.toml` manages `livekit`, `sandbox_token` and `example_utils`.
- Tags continue the existing format: `v{version}` for `livekit` and
  `{package}@v{version}` for the other components.
- A `livekit` release also updates the version in these files:
  - The four example manifests.
  - `README.md`.
  - `.github/ISSUE_TEMPLATE/bug_report.yml`.
  - `docs/Doxyfile`.

  These are the same files as the manual release PR #145.
- The workflows:
  - `release_pr.yml` keeps one release PR open.
  - `release.yml` tags, creates the GitHub release and uploads to the
    ESP Component Registry after the release PR merges.
  - `changeset_check.yml` requires a changeset on contributor PRs.
  - `pre_release.yml` opens an rc PR on demand. IDF accepts versions such
    as `0.4.0-rc.1`, so the default rc label works.
- The old `release.yml` uploaded every component on each push to `main`.
  The new `release.yml` uploads only after a release PR merges, or when
  you start it manually.
- The docs deploy on each push to `main` moved to the docs job of
  `ci.yml`, which already built the docs on that push. The github-pages
  environment accepts deploys from `main` only, so the deploy cannot move
  into `release.yml`, which runs on a PR event.
- `esp_registry.yml` stays. CI uses it for the dry-run registry check.
- Each managed component gets a `CHANGELOG.md` with its first verwalter
  release. The ESP Component Registry shows this file on the component
  page. Earlier release notes stay in the GitHub releases.

## Decisions

- **Vendored components are not managed** (`nanopb`, `khash`). Their
  versions follow upstream. When you vendor a new upstream version, set
  the version by hand. The publish job of `release.yml` uploads the
  committed version with `--allow-existing`. Thus a new version goes up
  with the next release, or when you start `release.yml` manually.
- **Missing tags are backfilled by hand.** `sandbox_token` 0.1.0 is on the
  registry but has no tag. Tag the commit that added it (a92e11d, #35):
  `git tag sandbox_token@v0.1.0 a92e11d && git push origin sandbox_token@v0.1.0`.
  `verwalter status` reports the package as untagged until you do this.
- **Dependency ranges are not rewritten.** The examples pin
  `sandbox_token ~0.1.0` and `example_utils ~0.2.0`. `livekit` pins
  `khash ~0.2.8` and `nanopb ~0.4.9~2`. In IDF, `~X.Y.Z` means
  `>=X.Y.Z,==X.Y.*`, so it accepts new patches. IDF checks the range
  even with `override_path`. If a 0.x component gets a minor bump, the
  example builds fail in CI until you edit the range by hand.
- **Registry authentication stays on the `ESP_REGISTRY_TOKEN` secret.**
  Possible follow-up: the registry supports GitHub OIDC, which removes the
  secret.
- **The release PR uses a GitHub App token**, so that CI runs on it.

## Limitations to raise upstream

1. **No IDF component support.** verwalter does not know
   `idf_component.yml`:
   - `[workspace] members` cannot find components, so each one is listed
     under `[packages]`.
   - The version needs a regex.
   - Dependencies between components are not read from the manifests,
     and their ranges are not updated. This is a gap because `livekit`
     depends on `khash` and `nanopb`.
2. **IDF revision versions do not parse.** `0.4.9~2` (the `~revision`
   field of IDF versions) stops `verwalter plan` with "invalid semantic
   version". This is the reason `nanopb` cannot be managed. Note: git
   tags cannot contain `~`, so IDF tags use `.` instead (`0.4.9.2`). A
   fix needs both the parser and the tag mapping.
3. **A regex that matches values that disagree gives no warning.** "Every
   match moves" is by design. Our first `bug_report.yml` regex also
   matched the IDF version placeholder (`v5.5`). `plan` reported nothing,
   and the release branch overwrote the IDF placeholder with the SDK
   version. A drift check over all matches would catch this.
4. **No glob in `versioning.files`.** The four example manifests take one
   entry each. A pattern such as `examples/*/main/idf_component.yml` would
   cover new examples automatically.
5. **The publish command cannot use the package path.** Only `{package}`,
   `{version}`, `{tag}` and `{npm_tag}` are substituted. The registry
   needs the path in the repository, so the command runs
   `git rev-parse --show-prefix` in a shell. A `{path}` placeholder would
   be simpler.
6. **No support for unmanaged packages.** A package that verwalter does
   not version, but that the publish job still uploads, needs a
   hand-written workflow step. An example is a vendored component with an
   upstream version.

## Handoff: steps for a maintainer

- [ ] Install the verwalter release GitHub App on this repository. Add the
      `VERWALTER_RELEASE_CLIENT_ID` and `VERWALTER_RELEASE_APP_KEY` secrets.
      Other LiveKit repositories already use this App.
- [ ] Create the `internal` label. Add the label to the migration PR.
- [ ] Push the `sandbox_token@v0.1.0` tag (see "Decisions").
- [ ] Tell contributors to add a changeset to each PR that changes a
      component. Use `verwalter changeset`, or add a file in `.changeset/`.
      On 0.x, a breaking change is `minor` and a new feature is `patch`.
