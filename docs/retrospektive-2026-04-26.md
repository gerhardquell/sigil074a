# Retrospektive 26.04.2026

## Erledigt

| # | Aufgabe | Commit | Status |
|---|---------|--------|--------|
| 1 | Cursor-Sync CodeView → Preview | `2e7a870` | Done |
| 2a | Translator: Whitelist JSON fix (trailing comma) | config | Done |
| 2b | Translator: WaitCursor + Statusmeldung | `409c6e2` | Done |
| 3 | Fehlerposition: ScrollToLine immer aufrufen | `24141f9` | Done |
| 4 | Qt5/Qt4-Referenzen → Qt 6.9 | `90bc2c8` | Done |

## Offen (ToDo.md)

- Translator: Fehlerbehandlung verbessern (kein Absturz bei Fehler)
- Translator: Anzeige des Übersetzungsvorgangs — evtl. Progress-Animation statt nur Statusmeldung

## Was gut lief

- Brainstorming → Spec → Plan → Subagent-Implementierung: sauberer Flow
- Code-Review fand `:nth-of-type` vs `:nth-child` Bug — hätte im Betrieb zu falschem Scrollen geführt
- Debounce 100ms → 200ms nach Performance-Review von `GetCaretLocation()`

## Was besser könnte

- Server-Readiness: sigoREST hatte beim ersten curl nur glm-Modelle — beim zweiten alle. Server war noch nicht ready.
- CLAUDE.md stand noch auf Qt 5.2 — hätte beim Session-Start auffallen müssen
- Translator-Whitelist-Fehler: "Invalid whitelist format" ist irreführend — eigentlich "JSON parse error". Bessere Fehlermeldung wäre hilfreich.

## Commits diese Session

- `90bc2c8` chore: update Qt5/Qt4 references to Qt 6.9
- `24141f9` fix: always scroll to error line after well-formedness check
- `409c6e2` feat(Translator): show wait cursor and status message during translation
- `2e7a870` feat: add cursor sync from CodeView to Preview
