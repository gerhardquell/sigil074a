# Retrospektive 22.06.2026

## Erledigt

| # | Aufgabe | Commit | Status |
|---|---------|--------|--------|
| 1 | Translate-Popup um "<p> EN→DE / DE→EN" erweitert | pending | Done |
| 2 | Batch-Übersetzung: alle `<p>` und `<li>` Blöcke nacheinander | pending | Done |
| 3 | README.md mit Batch-Übersetzung dokumentiert | pending | Done |

## Was gut lief

- Wiederverwendung der bestehenden `Translator`-Klasse und des `QSignalMapper`-Patterns — kein neuer Netzwerkcode nötig.
- Unterscheidung Einzelblock vs. Batch über das Präfix `all:` im Mapper-String — minimale Änderung an der Signatur.
- Positions-Strategie "immer neu suchen ab letzter Einfügeposition" vermeidet komplizierte Offset-Buchhaltung.

## Was besser könnte

- Kein Abbruch-/Cancel-Button während der Batch-Übersetzung. Nutzer muss warten oder ein neues Translate auslösen (abortiert via `Translator`).
- Keine Fortschrittsanzeige mit Prozent — nur Statusmeldung "Block N ...". Bei sehr großen Dokumenten unübersichtlich.
- Verschachtelte `<li> > <p>` werden noch nicht explizit behandelt; aktuell wird der äußere Block zuerst gefunden.

## Commits diese Session

- pending: feat(CodeView): add batch translation of all <p>/<li> blocks
- pending: docs: document batch paragraph/list translation in README.md
