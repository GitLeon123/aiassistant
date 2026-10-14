# AIAssistant: Phasenplan

Stand: 2026-10-08

Dieser Plan ordnet die Features aus [audience-research.md](audience-research.md) nach Wichtigkeit. Die wichtigsten kommen zuerst, optionale Features ganz am Ende.

## Wie priorisiert wurde

Ein Feature rückt nach vorn, wenn mehrere der folgenden Punkte zutreffen:

1. **Breite:** Es wurde von vielen der 7 Rollen genannt (siehe Abschnitt 1 der Recherche).
2. **Vertrauen und Risiko:** Fehlt es, würden Nutzer die App ablehnen (Deal-Breaker) oder es entsteht ein Datenschutzrisiko.
3. **Hebel:** Es entscheidet, ob jemand die App überhaupt zum Laufen bekommt oder sie weiter benutzt.
4. **Aufwand:** Kleine Änderungen mit großer Wirkung kommen vor großen Umbauten.

Aufwand: **S** = Stunden bis 1 Tag, **M** = 2 bis 4 Tage, **L** = über 1 Woche.

**Wichtig:** Die Rangfolge beruht auf den Berichten der Subagents. Das sind überwiegend Suchtreffer-Ausschnitte und eigene Einschätzungen, keine gemessene Nachfrage. Deshalb ist nach Phase 2 ein Test mit echten Nutzern eingeplant, bevor Phase 4 und später gebaut werden.

## Bestandsaufnahme vor Phase 1

Nur mit Stichproben im Code geprüft, nicht vollständig durchgesehen:

- Alt+Space, Hotkey ist änderbar. Schlägt die Registrierung fehl, steht ein Fehler im Einstellungsdialog (`App.cpp`, `RegisterCurrentHotkey` und `SetHotkey`).
- Die Leiste fordert per `WDA_EXCLUDEFROMCAPTURE` Capture-Ausschluss an (`WebWindow.cpp`); die Unterstützung muss je Tool geprüft werden.
- Kopier-Button an Antworten und an Codeblöcken, Strg+Umschalt+C kopiert die letzte Antwort.
- Screen-Chip mit `aria-pressed`, Modell-Picker mit ARIA-Rollen, Fehler mit `role="alert"`.
- Idle-Reset nach 10 Minuten, Verlauf nur im Speicher, Schlüssel im Credential Manager.

Nicht gefunden: KaTeX/LaTeX-Rendering, `aria-live`-Bereiche, `forced-colors`-Styles, Screenshot-Vorschau, Sperrliste für Fenster, Richtlinien-Datei.

---

## Phase 1: Vertrauen beim Screenshot (Pflicht vor jeder Veröffentlichung)

**Ziel:** Jeder Nutzer sieht vor dem Senden, was die App schickt und wohin. Das war der einzige Punkt, den alle 7 Rollen nannten, und er ist die Grundlage für alles Weitere.

| # | Feature | Aufwand | Rollen | Fertig, wenn |
|---|---|---|---|---|
| 1.1 | **Vorschau des Screenshots** vor dem Senden (kleines Vorschaubild, per Klick vergrößern, entfernbar) | M | alle | Bei eingeschaltetem Screen-Chip erscheint ein Vorschaubild mit Angabe wie „Monitor 2, 1920×1080". Entfernen schaltet den Chip aus. |
| 1.2 | **Ziel-Anzeige „Lokal / Cloud"** am Chip oder Sendeknopf, mit Anbieter und Modell | S | alle | Vor dem Senden steht z. B. „Screen → Claude (Cloud)" oder „→ Ollama (lokal)". |
| 1.3 | **Sichtbarer Aufnahmezustand** (kurzes Signal, dass gerade erfasst wurde) | S | Privatsphäre, Support | Beim Erfassen erscheint ein erkennbarer, kurzer Hinweis. |
| 1.4 | **Sperrliste für Fenster/Prozesse** (Passwortmanager, Banking, private Browserfenster, Remotedesktop), standardmäßig an | M | Privatsphäre, Büro, Support, Student | Ist ein gesperrtes Fenster aktiv, wird nicht erfasst und die Leiste sagt warum. Liste ist in den Einstellungen bearbeitbar. |
| 1.5 | **Prüfen, dass die Leiste in Windows Recall und gängigen Capture-Tools nicht auftaucht** | S | Privatsphäre | Manueller Test dokumentiert. `WDA_EXCLUDEFROMCAPTURE` ist laut Microsoft keine Sicherheitsgarantie, das steht so in der Doku. |
| 1.6 | **Telemetrie-Behauptung prüfen** (Paketmitschnitt), erst danach „keine Telemetrie" behaupten | S | Privatsphäre | Liste aller ausgehenden Verbindungen pro Modus (CLI, API-Schlüssel, lokal) liegt vor. |

**Reihenfolge innerhalb der Phase:** 1.2, 1.3, 1.5, 1.6 (klein), dann 1.1, dann 1.4.

**Implementierungsstand (2026-10-08):** 1.1–1.4 sind implementiert: vergrößerbare/entfernbare Vorschau der tatsächlich gesendeten Bildbytes, Anbieter/Modell mit Lokal-/Cloud-Ziel anhand des Endpunkts, Aufnahmehinweis und standardmäßig aktive, bearbeitbare Prozess-/Fenstertitel-Sperrliste. Für 1.5/1.6 liegen Prüfanleitung, Egress-Inventar aus dem Code und ein ehrlicher Ergebnisstatus in [privacy-verification.md](privacy-verification.md) vor. Die manuellen Capture-/Recall-Tests und Paketmitschnitte aller Modi stehen noch aus; Phase 1 bleibt bis dahin eine offene Veröffentlichungsschranke. Keine Behauptung „keine Telemetrie“.

---

## Phase 2: Erster Start und Einrichtung

**Ziel:** Jemand ohne Terminalkenntnisse kommt in unter einer Minute zur ersten Antwort. Die Recherche sieht das als größtes Annahmerisiko für Nicht-Entwickler, allerdings überwiegend als Schlussfolgerung und nicht durch Messdaten belegt.

| # | Feature | Aufwand | Rollen | Fertig, wenn |
|---|---|---|---|---|
| 2.1 | **Automatische Erkennung** vorhandener Anmeldungen (Claude Code, Codex, Gemini CLI) und lokaler Server (Ollama, LM Studio) | M | Büro, Support, Student | Beim ersten Start zeigt die App, was gefunden wurde, und schlägt einen Standard vor. |
| 2.2 | **Einrichtungsassistent** mit Testfrage | M | Büro, Support, Student | Der Assistent führt zu einem Anbieter und bestätigt per Testfrage, dass er antwortet. Klare Meldung, wenn Node/npm fehlt, mit Link statt Fachsprache. |
| 2.3 | **Hotkey-Konfliktprüfung beim ersten Start** mit Vorschlägen (z. B. Strg+Alt+Leertaste) | S | Entwickler, Büro, Kreative | Alt+Space gilt als konfliktträchtig (Systemmenü, PowerToys Run, Flow Launcher). Der Assistent warnt und bietet Alternativen an. |
| 2.4 | **Statusübersicht „Was ist verbunden"** in den Einstellungen | S | Support | Pro Anbieter: verbunden / nicht verbunden / nächster Schritt. |
| 2.5 | **Gewöhnlicher Installer** (kein Terminal nötig) | M | Büro, Support | Setup-Datei installiert, richtet Autostart optional ein und lässt sich deinstallieren. Signierung folgt in Phase 5. |

**Meilenstein nach Phase 2:** Erster Test mit 5 bis 8 echten Nutzern aus mindestens drei Gruppen (Entwickler, Büro, Student oder Familien-Nutzer). Danach wird entschieden, welche Teile von Phase 3 bis 6 vorgezogen oder gestrichen werden.

**Implementierungsstand (2026-10-08):** 2.1–2.5 sind implementiert: automatische CLI-/lokale Server-Erkennung mit Standardvorschlag, wiederaufrufbarer Einrichtungsassistent mit echter Text-Testfrage und Node.js-Downloadhinweis, Windows-geprüfte Hotkey-Alternativen samt Alt+Space-Warnung, Anbieterstatus mit nächstem Schritt sowie ein unsignierter Installer mit optionalem Autostart und Deinstallation. Bei Gemini gilt eine vorhandene OAuth-Datei nur als gespeicherte Anmeldung; erst die Testfrage bestätigt eine Antwort. Build, Unit-/HTTP-/Browser-Tests und Installations-/Deinstallations-Smoke-Test sind dokumentiert in [setup-verification.md](setup-verification.md). Die Ein-Minuten-Zielzeit, echte Provider-Anmeldungen und der Nutzertest sind noch nicht gemessen. Phase 1 bleibt eine offene Veröffentlichungsschranke.

---

## Phase 3: Lesbarkeit und Barrierefreiheit

**Ziel:** Die Glas-Leiste ist für Screenreader, bei Sehschwäche und mit Tastatur zuverlässig nutzbar. Die WCAG-Zuordnungen stammen aus dem Bericht und wurden nicht alle an Quellen geprüft. Vor dem Bau gegen die Spezifikation gegenprüfen.

| # | Feature | Aufwand | Rollen | Fertig, wenn |
|---|---|---|---|---|
| 3.1 | **Option „Feste Flächen"** (opake Fläche hinter Text), unabhängig von Windows-Transparenz | S | Barrierefreiheit, Support | Text hat auf jedem Hintergrund ausreichend Kontrast (4,5:1 für Text, 3:1 für Rahmen und Fokusring). |
| 3.2 | **`forced-colors`-Stylesheet** (Windows-Kontrastmodus) | S | Barrierefreiheit | Alle Steuerelemente haben im Kontrastmodus sichtbare Rahmen und Systemfarben. |
| 3.3 | **Status per `aria-live="polite"`**: „Antwort wird erstellt", „Antwort fertig", Fehler. Der Antworttext selbst wird nicht Token für Token angekündigt | M | Barrierefreiheit | Mit NVDA und Narrator geprüft: keine Flut an Fragmenten, das Ende ist hörbar. |
| 3.4 | **Fokus-Verwaltung**: Fokus kommt beim Öffnen ins Eingabefeld und kehrt beim Schließen zurück. Leiste verdeckt das fokussierte Element nicht | M | Barrierefreiheit | Mit Screenreader getestet. |
| 3.5 | **Zustand nicht nur über Farbe** (Screen-Chip mit Text oder Symbol), Zielgröße mindestens 24×24 px | S | Barrierefreiheit | Prüfliste abgehakt. |
| 3.6 | **Schriftgröße einstellbar**, Test bei Windows-Skalierung 200 % | S | Barrierefreiheit, Support | Größerer Standardtext für Dialoge, kein Abschneiden bei 200 %. |
| 3.7 | **Stopp-Knopf mit Beschriftung** und Pin-Schalter, der das automatische Schließen bei Fokusverlust verhindert | S | Barrierefreiheit | Beschriftete Schaltfläche, Pin-Zustand bleibt erhalten. |

---

**Implementierungsstand (2026-10-08):** 3.1–3.7 sind im Code umgesetzt: unabhängige opake Flächen, Systemfarben/Rahmen im Kontrastmodus, ruhige Statusansagen statt Token-Fragmenten, Fokusführung mit Modal-Picker und Rückkehr zum Auslöser, explizites Screen-an/aus, relative/einstellbare Schriftgrößen sowie beschrifteter Stopp und gespeicherter Pin. Browser-/axe-Tests und 200-%-Textgrößenprüfung sind dokumentiert in [phase34-verification.md](phase34-verification.md). NVDA/Narrator, tatsächliche Windows-DPI-Skalierung und Kontrast-Themes in WebView2 sowie die native Fokus-Rückkehr müssen noch manuell geprüft werden; keine WCAG-Konformitätsbehauptung.

---

## Phase 4: Antwortqualität und sauberes Einfügen

**Ziel:** Antworten sind lesbar, nachvollziehbar und lassen sich dort einfügen, wo man sie braucht.

| # | Feature | Aufwand | Rollen | Fertig, wenn |
|---|---|---|---|---|
| 4.1 | **Mathe-Rendering** (`$…$` und `$$…$$`) ohne Flackern während des Streamens, Kopieren als LaTeX | M | Student, Entwickler | Formeln erscheinen gerendert, Rohcode nie dauerhaft sichtbar. Bibliothek wählen (z. B. KaTeX), Größe und Sicherheit (DOMPurify) prüfen. |
| 4.2 | **Modell und Anbieter pro Antwort sichtbar** | S | Entwickler, Support | Unter jeder Antwort steht das antwortende Modell. |
| 4.3 | **Kopieren in Formatierung**: Rich Text für Outlook/Word, tabulatorgetrennt für Excel | M | Büro, Student, Entwickler | Eine Tabelle lässt sich in Excel in Zellen einfügen, Text behält Überschriften in Word. |
| 4.4 | **Hinweis auf Nutzungsgrenzen** bei Abo-CLIs und optional Ausgaben-Limit bei API-Schlüsseln | M | Entwickler, Student | Warnung vor dem Limit bzw. Tagesgrenze einstellbar. Wie genau sich CLI-Verbrauch ermitteln lässt, ist offen und muss je CLI geklärt werden. |
| 4.5 | **„Ich bin nicht sicher"-Verhalten** für Hochrisiko-Fragen (Betrug, Rechnungen, Menüpfade) per Systemprompt | S | Support, Kreative, Entwickler | Stichproben mit bekannten Fehlfragen zeigen weniger erfundene Menüpfade. |

---

**Implementierungsstand (2026-10-08):** 4.1–4.5 sind umgesetzt: lokales KaTeX mit stabilen Streaming-Formeln, LaTeX-Kopie und DOMPurify; Anbieter/angefordertes Modell pro Antwort; HTML/Text-Kopie und Excel-TSV; CLI-Limithinweis plus gemeldete Claude-Warnungen und optionaler nativer Tages-Allowance-Guard; vorsichtiger Standard-Systemprompt für Menüpfade, Betrug und Rechnungen. Der API-Guard reserviert einen vom Nutzer gewählten Betrag pro Anfrage und misst/begrenzt keine tatsächliche Anbieterrechnung. Verbleibende CLI-Quota ist über die aktuellen Bridges unbekannt. Native/UI-/HTTP-/Browser-Prüfungen sind in [phase34-verification.md](phase34-verification.md) erfasst. Echte Office-Paste-Tests, Screenreader-Abnahme und Wirksamkeits-Stichproben mit Live-Modellen stehen noch aus. Die bisherige Nutzerstudie und Phase-1-Veröffentlichungsschranke bleiben offen.

---

## Phase 5: Verteilung und Unternehmenstauglichkeit

**Ziel:** IT-Abteilungen können die App freigeben. Das ist nur nötig, wenn Firmen eine Zielgruppe sind. Der Aufwand ist hoch und kann gestrichen werden, wenn der Fokus auf Privatnutzern liegt.

| # | Feature | Aufwand | Rollen | Fertig, wenn |
|---|---|---|---|---|
| 5.1 | **Signierter Installer** (MSI/MSIX), SmartScreen und Smart App Control kommen damit klar | M | Privatsphäre, Büro | Ein unsigniertes Binary wird von Smart App Control blockiert. Signierdienst und Kosten vorher prüfen, die Zahl aus dem Bericht stammt aus einer Suchsnippet-Quelle. |
| 5.2 | **Silent-Install-Schalter und winget-Manifest** mit SHA256 | S | Privatsphäre | `winget install` funktioniert. winget verlangt, dass der Installer von der Seite des Herausgebers kommt. |
| 5.3 | **Richtlinien-Datei / ADMX** (z. B. `disableScreenCapture`, `allowedProviders`, `forceLocalOnly`, feste Idle-Zeit) | L | Privatsphäre, Büro | HKLM-Werte überstimmen Benutzereinstellungen, die Oberfläche zeigt gesperrte Optionen als gesperrt. |
| 5.4 | **Datenfluss-Dokument** mit Egress-Tabelle pro Modus, Unterauftragsverarbeitern und Aufbewahrung | M | Privatsphäre | Dokument im Repo, Angaben pro Anbieter auf Primärquellen geprüft. Keine Rechtsberatung. |
| 5.5 | **Audit-Log ohne Inhalte** (Zeit, Anbieter, Screen ja/nein, Richtlinienstatus) als JSON | M | Privatsphäre | Log enthält keine Nachrichtentexte oder Bilder. |
| 5.6 | **Dokumentation, was jede CLI selbst speichert** (die 10-Minuten-Regel der App steuert das nicht) | S | Privatsphäre | Pro CLI geprüft und dokumentiert. Im Bericht nur als Vermutung. |
| 5.7 | **Hinweis zu Privatkonten im Firmenumfeld** (CLI-Abo nutzt oft persönliche Konten) | S | Privatsphäre | Im Einrichtungsassistenten und in der Doku erklärt. |

---

## Phase 6: Mehr Kontext, weniger Tippen

**Ziel:** Bessere Eingaben für Fragen, bei denen ein ganzer Bildschirm zu ungenau oder zu teuer ist.

| # | Feature | Aufwand | Rollen | Fertig, wenn |
|---|---|---|---|---|
| 6.1 | **Bereichsauswahl** statt ganzer Bildschirm (Rahmen aufziehen) | M | Student, Entwickler, Kreative | Kleiner Ausschnitt wird gesendet, weniger Tokens, Text bleibt lesbar. |
| 6.2 | **Bild aus der Zwischenablage einfügen** | S | Entwickler, Student | Strg+V mit Bild hängt es an, mit Vorschau wie in 1.1. |
| 6.3 | **Ausgewählten Text als Kontext** (Tastenkürzel „Zu Auswahl fragen") | M | Büro, Entwickler | Funktioniert in Excel-Zellen und E-Mail-Text. Bei Apps ohne Unterstützung klarer Hinweis. |
| 6.4 | **Vorab-Schwärzung** von Kartennummern, Kontonummern, Schlüssel-Präfixen mit Vorschau | L | Support, Privatsphäre, Büro | Muster werden vor dem Senden maskiert und in der Vorschau markiert. Trefferquote ist begrenzt, in der Oberfläche nicht als Garantie darstellen. |
| 6.5 | **Vollbild-Erkennung**: In Spielen/Vollbild-Apps Erfassung standardmäßig aus | M | Kreative | Randloses Fenster ist das realistische Ziel, exklusives Vollbild zeigt meist keine normalen Overlays. |
| 6.6 | **Mehrere Monitore und hohe DPI** testen (Capture-Fehler sind ein Deal-Breaker für Entwickler) | S | Entwickler | Testmatrix mit mind. 2 Monitoren und 150 %/200 % Skalierung. |

---

## Phase 7: Optional (nur bei Bedarf nach Nutzertests)

Diese Punkte sind Einschätzungen der Subagents. Jeder sollte erst gebaut werden, wenn echte Nutzer ihn verlangen.

**Voreinstellungen und Modi**
- **Slash-Befehle:** `/explain`, `/regex`, `/ps` (nur Kopieren, nie automatisch ausführen). Aufwand S.
- **Betrugs-Check-Voreinstellung** mit fester Checkliste (Dringlichkeit, unbekannter Absender, Links, Fernzugriff, Geschenkkarten) und Hinweis, nur über offizielle Kanäle zu prüfen. Aufwand S.
- **Lernmodus** („Tipp statt Lösung") für Studierende. Aufwand S.
- **Einfacher-Modus** („Erkläre einfacher") und Antworten mit Ein-Satz-Zusammenfassung vorweg. Aufwand S.
- **Pro-App-Profile** (z. B. Excel, Outlook, Blender, VS Code) mit eigenen Voreinstellungen. Aufwand M.
- **Spielmodus**, der Inferenz pausiert und die Leiste ausblendet. Aufwand M.

**Ausgabe und Teilen**
- **Vorlesen** der letzten Antwort. Aufwand M.
- **„Zusammenfassung für meinen Helfer"** (Symptom, Programm, Version). Aufwand S.
- **Export als Markdown**, optional mit Verlauf, wenn Nutzer das wünschen. Aufwand S.
- **Karteikarten** aus einer Antwort oder Folie. Aufwand M.
- **Zitat-Prüfung** gegen Crossref oder Ähnliches. Aufwand L.

**Modelle und Verwaltung**
- **Vergleich zweier Modelle** nebeneinander. Aufwand M.
- **VRAM-Anzeige und automatisches Entladen lokaler Modelle**, wenn ein Spiel startet. Aufwand M.
- **Selbstmessung des Overheads** im Tray (Speicher, CPU). Aufwand S.
- **„Netzwerkaufrufe"-Anzeige** und erzwungener Lokal-Modus mit Endpunktliste. Aufwand L.
- **DLP-Anbindung** (z. B. Purview). Aufwand L, Machbarkeit ungeprüft.
- **An Terminal senden / Frage an Claude Code oder Codex mit aktuellem Ordner.** Aufwand M, überschneidet sich mit der bewussten Entscheidung, die CLIs ohne Werkzeuge laufen zu lassen. Erst entscheiden, ob das zum Produkt passt.
- **Thematische Prompt-Pakete** für Blender, Photoshop, Resolve. Jeder Menüpfad muss vor Auslieferung geprüft werden.

**Marketing-Regel:** Keine Formulierungen wie „unsichtbar" oder „nicht erkennbar". Studierende und Prüfungsanbieter lesen das als Betrugswerkzeug (siehe Recherche, Student und Kreative).

---

## Übersicht

| Phase | Thema | Warum an dieser Stelle |
|---|---|---|
| 1 | Vertrauen beim Screenshot | Von allen 7 Rollen genannt, Voraussetzung für Vertrauen |
| 2 | Einrichtung | Entscheidet, ob Nicht-Entwickler überhaupt starten |
| – | **Nutzertest** | Rangfolge ab Phase 3 an echten Daten prüfen |
| 3 | Barrierefreiheit | Glas-UI hat bekannte Kontrastrisiken, viele Punkte sind klein |
| 4 | Antwortqualität | Mathe, Einfügen und Modellanzeige steigern den täglichen Nutzen |
| 5 | Unternehmen | Hoher Aufwand, nur nötig wenn Firmen Zielgruppe sind |
| 6 | Mehr Kontext | Verbessert Eingaben, ist aber kein Muss |
| 7 | Optional | Nur nach nachgewiesenem Bedarf |

Phase 3 und 4 können parallel laufen, weil sie verschiedene Dateien betreffen (3 vor allem Stile und ARIA in `ui/src/bar`, 4 vor allem `ui/src/lib/markdown.ts` und das Antwort-Rendering).

## Offene Fragen

- **Zielgruppe:** Sind Firmen eine echte Zielgruppe? Davon hängt ab, ob Phase 5 überhaupt gebaut wird.
- **Preis- und Verteilungsmodell:** Kostenlos oder bezahlt? Das beeinflusst Signierung (5.1) und Kostenanzeigen (4.4).
- **CLI-Verbrauch:** Die aktuellen Headless-Bridges liefern keine verlässliche verbleibende Abo-Quota. Claude-Warn-/Ablehnungs-Events werden ausgewertet; Codex/Gemini bleiben bei einem ehrlichen Hinweis auf unbekannte Quota. Ein späterer Quota-Snapshot braucht eine dokumentierte Schnittstelle.
- **WebView2/Windows-Abnahme:** `forced-colors` ist in Edge per Emulation geprüft; reale Windows-Kontrast-Themes, NVDA/Narrator und 200-%-DPI in WebView2 stehen als manuelle Abnahme aus.
