# Security Policy

## Supported Versions

| Version | Supported          |
| ------- | ------------------ |
| 0.1.x   | :white_check_mark: |

## Reporting a Vulnerability

Please report security vulnerabilities privately via email to
**leminhtri11a1@gmail.com**.

Do **not** open a public issue for security bugs.

### What to include

- A clear description of the vulnerability
- Steps to reproduce (input sequences, application, frontend)
- Affected versions or commit hash
- Whether the issue exposes typed text, user data, or system state
- Any suggested mitigation or fix

### Response timeline

- Acknowledgment within 3 business days
- Initial assessment within 7 business days
- Fix or mitigation plan within 30 days for critical issues
- Public disclosure after a fix is released and users have had time to upgrade

## Security design notes

fcitx5-bami is an input method engine with strict privacy requirements:

- Typed text is **never** logged to disk or transmitted unless an explicit debug
  flag is set.
- Learning is **disabled by default**.
- Learning in password, URL, and email fields is **forbidden**.
- User data files are written with mode `0600` inside a `0700` directory,
  atomically.
- All composition happens locally; no cloud service receives keystrokes.
- The key path contains no timers, sleeps, D-Bus calls, file I/O, or network
  access.

If you discover a way to bypass any of these guarantees, please report it as a
security issue.
