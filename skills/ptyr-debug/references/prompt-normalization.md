# Prompt Normalization

Use this reference before writing the scenario comment inside the regression test.

## Goal

Keep the user scenario close to verbatim while correcting obvious language errors.

## Rules

- Preserve meaning and sentence order.
- Correct spelling and clear typos.
- Fix punctuation and capitalization for readability.
- Avoid adding new facts or assumptions.
- Keep domain wording unchanged, for example "default terminal", "numbered choice question", and "overlay".

## Comment Template

Use a short block comment at the top of the new test:

```c
/*
Scenario (from user prompt, spelling/typos corrected):
I started the app and the default terminal showed a numbered choice question, but I saw no overlay in the app.
*/
```

## Acceptable Transformations

- "analze recording 17" -> "analyze recording 17"
- "I saw no overlay in app" -> "I saw no overlay in the app."
- Keep "recording 17" exactly as provided.

