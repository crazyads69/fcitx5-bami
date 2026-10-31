# Pull Request

## Summary
<!-- Describe the change in one or two sentences. -->

## Motivation
<!-- Why is this change needed? Link to any relevant issues. -->

## What was changed
<!-- List the files modified and the nature of the change. -->

## Testing
<!-- How did you verify the change? List commands run and results. -->

- [ ] `go vet ./... && go test -race -count=1 ./...` passes
- [ ] `ctest --test-dir build --output-on-failure` passes
- [ ] New logic has a regression test
- [ ] Docs updated (`README.md`, `docs/*.md`, or inline comments)
- [ ] No timers, sleeps, or file I/O added to the key path
- [ ] Privacy: no typed text logged unless under an explicit debug flag

## Checklist

- [ ] I have read `CONTRIBUTING.md`
- [ ] My change is limited to the files I own or have coordinated with other
      contributors (see `.codex/agents/*.toml` for file ownership)
- [ ] I have verified my change in at least one container (Ubuntu 22.04,
      Debian 12, or Ubuntu 24.04)
- [ ] Known limits or open questions are documented in `docs/LIMITS.md`
