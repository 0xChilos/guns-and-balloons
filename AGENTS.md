# Instructions for agents

Unreal Engine project for the game **Guns and Balloons** (game jam). The `Content/` layout is described in [README.md](README.md).

## Design documentation
- The source of truth for design is [`docs/`](docs/README.md). Start with `docs/README.md` and `docs/overview.md`, then read only the pages you need from `docs/features/`.
- Before implementing a feature, check its page: rules, parameters, open questions. Don't invent `TBD` values: put them in a constant/DataAsset and mark them as needing tuning.
- If the implementation changes the design, update the page (status, date, parameters) and add a row to `docs/decisions.md`.
- `docs/features/` holds only approved design. Write new ideas and changes to `docs/decisions.md` with the status `Proposed`; don't implement proposals without a human's confirmation. Move them to the feature page only after approval.
- A new feature = a copy of `docs/_template.md`, at most one page of text, plus a link in the table of contents in `docs/README.md`.
- `docs/source/` holds the original raw notes; don't change their content.
- **All documentation is written in English.** Chat with the team may be in Ukrainian, but everything committed to the repo (docs, code comments, commit messages) is English.
