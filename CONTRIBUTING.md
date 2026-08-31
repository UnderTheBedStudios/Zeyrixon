# Contributing to Zeyrixon

Thank you for your interest in contributing to Zeyrixon — contributions of all kinds are welcome.

Please read this document before opening issues or pull requests.

## Code of Conduct

By participating, you agree to abide by our [Code of Conduct](CODE_OF_CONDUCT.md).

## How to contribute

- Open an issue to propose changes or discuss major new features before implementing.
- Create a topic branch for your work, named clearly (e.g., `feature/shader-rework`, `fix/build-windows`).
- Keep pull requests focused and small. One logical change per PR speeds review.

## Pull request process

1. Fork the repository and create a branch from `master`.
2. Make your changes and ensure the project builds locally.
3. Run any tests and linters (see below) and include relevant test cases if applicable.
4. Push your branch to your fork and open a PR against `master`.
5. Fill the PR template (if present) and describe what, why, and how to test.
6. Address reviewer feedback; maintainers will merge once approvals are satisfied.

## Coding style

- Follow existing project conventions. Keep changes consistent with surrounding code.
- Prefer clear names and small functions.

## Commit messages

- Use descriptive commit messages. Example style:

```
feat: add shader hot-reload
fix: correct texture binding in Renderer
docs: update README build instructions
```

- Consider using Conventional Commits for easier changelog generation.

## Tests

- If you add functionality, include tests when practical.
- Describe how to run tests locally in the PR description.

## Building & development

- Run the generator script before building:

```bash
# Linux
./GenerateProjects.sh

# Windows
GenerateProjects.bat
```

- Build with `make config=debug` (or `release`/`dist`) or open the generated solution in Visual Studio.

## Reporting issues

- Create issues with a clear title and reproduction steps.
- Attach logs, compiler output, and platform details (OS, compiler, config).

## Licensing and CLA

By contributing you agree that your contributions will be licensed under the project's MIT License. If you need a separate contributor license agreement (CLA) we will state it here.

## Thank you

Thanks for helping improve Zeyrixon! If you'd like help getting started, open an issue and tag it `good-first-issue`.