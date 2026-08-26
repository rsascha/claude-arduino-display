# Schaltplan — was tatsächlich verdrahtet ist

Auswertung des **offiziellen Elecrow-Schaltplans** für das CrowPanel ESP32 5.79" E-Paper
(SKU DIS08792E). Bis hierher stammte die GPIO-Zuordnung dieses Repos ausschließlich aus
dem Beispielcode (`material/CLAUDE.md` §4 und §5 sagen das ausdrücklich). Dieses Dokument
ersetzt die Rückschlüsse an den Stellen, wo der Netzplan eine Antwort gibt — und benennt
die Stellen, wo auch er schweigt.

**Maßgeblich sind die Dateien in `schaltplan/`**, nicht diese Zusammenfassung. Jede Aussage
hier nennt deshalb das Netz oder das Bauteil, aus dem sie stammt.

## Herkunft

| | |
|---|---|
| Quelle | Elecrow-Wiki → *Schematic & PCB* → [`CrowPanel-ESP32-Display-5.79E-Inch.zip`](https://www.elecrow.com/download/product/CrowPanel/E-paper/5.79-DIS08792E/CrowPanel-ESP32-Display-5.79E-Inch.zip) |
| Format | Autodesk Eagle 9.6.2 — `.sch` (Schaltplan), `.brd` (Layout), `.pdf` (Plot) |
| Board-Revision | **V1.0** (Silkscreen `V1.0`, `SKU:DIS08792E`), Dateidatum 09/2024 |
| Abgelegt in | `material/schaltplan/` |

Das PDF ist ein reiner Vektorplot **ohne Textebene** — `pdftotext` liefert null Zeilen.
Alle Angaben unten kommen deshalb aus dem XML der `.sch` und `.brd`.

**Fremdmaterial.** Die Dateien gehören Elecrow und dürfen nicht weiterverbreitet werden.
Sie fallen unter denselben Vorbehalt wie die Datenblätter in `material/` — siehe
`CLAUDE.md` → *Stand*: Wird das Repo je öffentlich, müssen sie vorher raus und durch den
Link oben ersetzt werden.

### Netzliste zum Durchsuchen

`schaltplan/netlist.txt` enthält alle 78 Netze mit ihren Anschlüssen, ein Netz je Zeile,
erzeugt aus der `.sch`:

```bash
grep -E '^(BAT|3V3|LCD_GND|IO7)' material/schaltplan/netlist.txt
```

```python
# so entstanden (Python 3, nur Standardbibliothek)
import xml.etree.ElementTree as ET
from collections import defaultdict
r = ET.parse('CrowPanel ESP32 Display-5.79(E) Inch.sch').getroot()
nets = defaultdict(set)
for net in r.iter('net'):
    for pr in net.iter('pinref'):
        nets[net.get('name')].add((pr.get('part'), pr.get('pin')))
for n in sorted(nets):
    print(f"{n}: " + ", ".join(f"{p}.{pin}" for p, pin in sorted(nets[n])))
```

## Kurzantworten

| Frage | Antwort | Beleg |
|---|---|---|
| BAT-Polarität | Pad 1 = **Plus**, aufgedruckt `+`/`−` | `B1`, Layer 21 |
| Akkuspannung messbar? | **Nein** — kein Teiler, kein ADC-Pin am Netz `BAT` | Netz `BAT` |
| 3V3/GND am Header? | Ja, **je 4 Pins**; Strombelastbarkeit nirgends genannt | Netze `3V3`, `GND` |
| Display-Enable? | Ja, **IO7** — schaltet die **Masse**, nicht die Versorgung | Netz `LCD_GND` |
| Deep-Sleep-Strom? | **Nicht dokumentiert**, muss gemessen werden | – |
| IO19/IO21 frei? | **Ja**, USB-Buchse hängt allein am CH340 | Netze `IO19`, `USB_D+` |
| IO3 nutzbar? | Ja, völlig unbeschaltet — aber Strapping-Pin | Netz `IO3` |
| RTC mit Puffer? | **Nein**, weder Baustein noch Halter noch Uhrenquarz | Stückliste |
| Display-Bibliothek | Elecrow-eigen, Software-SPI; Partial Refresh vorhanden | `examples/` |

## Stromversorgungspfad

```
USB-C (J2) ──VBUS──┬── U1  CH340C            (nur bei USB bestromt)
                   ├── U6  4054-Lader ──BAT──┬── B1  Akkustecker
                   ├── R10/R11 ─► Gate Q3    ├── C5, C6  je 10 µF
                   └── D2 (S2M) ──┐          ├── D3 (SS12) ──┐
                                  │          └── Q3 (PMOS) ──┤
                                  └────────── VIN ───────────┘
                                               │
                                     U2  RY3420 Step-Down
                                     L1 5,6 µH, FB: R13 45,3k / R14 10k
                                               │
                                             3V3 ──┬── U3  ESP32-S3-WROOM-1
                                                   ├── U4  Display VCI + VDDIO
                                                   ├── U5  Header, 4 Pins
                                                   └── J1  TF-Karte VDD
```

**Pfadumschaltung.** `Q3` ist ein PMOS (`PMOS-3401-4A`) zwischen `BAT` und `VIN`; sein Gate
hängt am Teiler `R10` (1 k) / `R11` (10 k) an `VBUS`. Liegt USB an, ist das Gate hoch, der
PMOS sperrt, und der Akku ist vom System getrennt — versorgt wird dann über `D2` aus
`VBUS`. Ohne USB zieht `R11` das Gate auf Masse, der PMOS leitet, der Akku speist `VIN`.
`D3` (SS12) liegt parallel als Notpfad.

**Für den Akkubetrieb ist das eine gute Nachricht:** Der Teiler `R10`/`R11` und der
CH340 hängen an `VBUS`, nicht am Akku. Ohne USB zieht dort nichts.

**Regler `U2` = RYCHIP RY3420**, laut Herstellerdatenblatt ein synchroner Step-Down,
1,2 MHz, **max. 2 A**. Der Rückkopplungsteiler `R13`/`R14` ergibt mit den üblichen 0,6 V
Referenz `0,6 × (1 + 45,3/10) = 3,32 V` — *erschlossen*, die Referenzspannung steht nicht
im Plan.

**Laderegler `U6`** (`DFN8_4054A`), `PROG` über `R12` = 2 kΩ nach Masse. Das ergäbe bei
einem LTC4054/TP4054 (`I = 1000 × V_PROG / R_PROG`, V_PROG = 1 V) rund **500 mA**
Ladestrom, bei einem TP4056 (`1200 V / R`) rund 600 mA. *Erschlossen* — welche Variante
tatsächlich bestückt ist, sagt der Plan nicht (`U6` trägt keinen Herstellernamen).

**Kein Tiefentladeschutz auf der Platine.** Weder DW01 noch vergleichbarer Baustein ist
bestückt; der 4054 schützt nur beim Laden. Der Akku muss seine eigene Schutzelektronik
mitbringen.

## Akkuanschluss `B1` — Polarität

Package `SH1.0MM-W`, vier Pads:

| Pad | Netz | Bedeutung |
|---|---|---|
| **1** | `BAT` | **Plus** |
| 2 | `GND` | Minus |
| 3, 4 | `GND` | Befestigungslaschen |

Im Layout (`.brd`) sitzt `B1` bei x = 178,84 / y = 5,32 mm. Die Signalpads liegen bei
x = 178,34 (Pad 1) und x = 179,34 (Pad 2), y = 7,72. Im Bestückungsdruck (Layer 21) stehen
darüber:

| Zeichen | x | y |
|---|---|---|
| `+` | 177,84 | 8,01 |
| `−` | 179,84 | 8,08 |

**Die Polarität ist also aufgedruckt**, das `+` steht beim Pad, das der USB-C-Buchse
näher liegt (`J2` bei x = 162,91 an derselben Kante). Blickrichtung: Bestückungsseite,
also die Rückseite mit ESP32-Modul und USB-Buchse.

Das ändert nichts an der Warnung in `CLAUDE.md` → *Akkuanschluss*: Die Verwechslungsgefahr
sitzt im **Akkukabel**, nicht auf der Platine. Vor dem ersten Anstecken messen.

## Akkuspannung messen — geht nicht ohne Umbau

Das Netz `BAT` hat genau diese Mitglieder:

```
BAT: B1.B+, C5.2, C6.2, D3.A, P5.P$1, Q3.D, U6.BAT
```

Akkustecker, zwei Stützkondensatoren, Schottky nach `VIN`, ein Testpad, der PMOS und der
Lader. **Kein GPIO, kein Spannungsteiler.** Es gibt im gesamten Plan keinen Teiler auf
einen ESP32-Pin — der Ladestand ist ohne Zusatzbeschaltung nicht messbar.

### Nachrüsten

Ein Teiler vom Akkunetz auf einen ADC-Pin des Headers. Auswahl des Pins nach ADC-Block
(ESP32-S3-Datenblatt, Tabelle 3):

| Header-Pin | ADC | mit WLAN nutzbar |
|---|---|---|
| IO3 | ADC1_CH2 | ja — aber Strapping-Pin |
| **IO8** | **ADC1_CH7** | **ja** |
| IO9 | ADC1_CH8 | ja |
| IO14…IO20 | ADC2_CH3…CH9 | **nein** — ADC2 ist blockiert, solange WLAN läuft |
| IO21, IO38 | – | kein ADC |

Vorschlag: 2 × 1 MΩ von `BAT` nach `GND`, Mittelabgriff auf **IO8**. Bei 4,2 V sind das
2,1 µA Dauerlast und 2,1 V am Pin — mit 11-dB-Dämpfung im Messbereich. Wer die 2 µA
sparen will, schaltet den Fußpunkt des Teilers über einen kleinen NMOS, genau wie es
`Q9`/`Q10` auf dem Board vormachen.

**Abgriff nicht am Testpad `P5`.** Alle 49 `P*`-Pads liegen im Layout gespiegelt (`rot="MR0"`),
sitzen also auf der **Displayseite** der Platine und sind unter dem Panel nicht erreichbar.
Erreichbar auf der Bestückungsseite sind stattdessen Pad 1 von `B1` selbst sowie `C5`/`C6`
(0805, x = 181,27 bzw. 183,11 / y = 11,73).

## 2×10-Header `U5`

Der Silkscreen beschriftet jeden Pin einzeln. Reihenfolge längs des Steckers, Paare
übereinander:

| | | | | | | | | | |
|---|---|---|---|---|---|---|---|---|---|
| IO3 | IO9 | IO15 | IO17 | IO19 | IO21 | GND | GND | GND | GND |
| IO8 | IO14 | IO16 | IO18 | IO20 | IO38 | 3V3 | 3V3 | 3V3 | 3V3 |

Netzseitig: `3V3` an `U5.P$11…P$14`, `GND` an `U5.P$7…P$10`.

**Eine Strombelastbarkeit nennt keine Quelle** — nicht das Handbuch, nicht das Wiki, keine
Notiz im Plan. Die Obergrenze ergibt sich aus `U2` (2 A) abzüglich dessen, was Board und
Panel selbst ziehen; WLAN-Spitzen des Moduls liegen bei 355 mA (WROOM-1 S. 17). Im
Akkubetrieb kommt hinzu, dass der Step-Down aus 3,0–4,2 V speist und bei leerem Akku in
den Durchgriff geht. *Erschlossen, keine Herstellerangabe.*

## Display-Freigabe IO7 — ein Masseschalter

Der wichtigste Befund für Deep Sleep:

```
IO7_LCD_3.3_CTL: R172.1, U3.IO7          R172 = 100 kΩ
N$7:             Q9.G, R172.2            Q9 = 2N7002 (NMOS)
LCD_GND:         Q9.D, U4.VSS, U4.BS, R18.1, R24.1, C20…C29, D5.C, …
GND:             Q9.S, …
3V3:             U4.VCI, U4.VDDIO, …
```

IO7 schaltet über 100 kΩ das Gate von `Q9`, und `Q9` verbindet die **Masse** des Panels
(`LCD_GND`) mit GND. `VCI` und `VDDIO` des Displays liegen **fest am 3V3-Netz** und werden
nie abgeschaltet.

Damit ist `IO7 = LOW` funktional trotzdem ein vollständiges Abschalten: ohne Rückstrompfad
fließt nichts, auch nicht durch die Boost-Erzeugung des Panels (`Q4` Si1308EDL, `L2` 47 µH,
`D4`/`D5`/`D6`, Netze `VSH`, `VSL`, `VCOM`, `VDHR`, `PREVGH`, `PREVGL`).

Zwei Konsequenzen:

- **Am Gate fehlt ein Pulldown.** Nur die 100 kΩ zu IO7. Geht der Pin im Deep Sleep in
  Hi-Z, ist das Gate undefiniert. IO7 ist RTC-fähig (`RTC_GPIO7`), also vor dem Schlafen
  ausdrücklich LOW setzen und mit `gpio_hold_en()` bzw. `rtc_gpio_hold_en()` halten.
- **Dieselbe Schaltung ein zweites Mal für die TF-Karte:** `IO42 → R15 (100 kΩ) → Q10 →
  TF_GND`. Auch die Karte hängt nur über ihre Masse.

Nebenbei aus demselben Block: `U4.BS` liegt auf `LCD_GND`, also fest auf 0 — das wählt am
SSD1683 **4-Draht-SPI**. Und `U4.TSCL`/`U4.TSDA` (der externe Temperatursensor-Bus des
Controllers) haben nur Pull-ups `R25`/`R26` (4,7 kΩ) und gehen an **keinen** ESP32-Pin;
dieser Bus ist auf diesem Board unbenutzt.

## Deep-Sleep-Bilanz

Eine Verbrauchsangabe für den Schlafzustand **nennt keine Elecrow-Unterlage**. Was sich
aus den Datenblättern und dem Plan zusammensetzen lässt:

| Beitrag | Wert | Quelle |
|---|---|---|
| ESP32-S3-WROOM-1, Deep Sleep | 7–8 µA | WROOM-1 S. 18, Tab. 14 |
| SSD1683 Deep Sleep (VCI, DC/DC aus) | 1–5 µA je Controller | SSD1683 S. 44, `Idslp_VCI1/2` |
| Panel bei IO7 = LOW | ≈ 0 — Masse getrennt | Netz `LCD_GND` |
| CH340 `U1` | 0 ohne USB — hängt an `VBUS` | Netz `VBUS` |
| Power-LED `D1` | aus, solange IO41 LOW (`R17` = NC) | Netz `N$25` |
| Laderegler `U6` | an `VBUS`, ohne USB nur Leckstrom | Netz `VBUS` |
| Gate-Teiler `R10`/`R11` von `Q3` | an `VBUS`, belastet den Akku nicht | Netz `VBUS` |
| **Ruhestrom `U2` (RY3420)** | **unbekannt — dominiert vermutlich** | – |

Das Board ist für Batteriebetrieb also sauber aufgebaut; der einzige offene Posten ist der
Ruhestrom des Step-Down-Reglers. **Das ist die Zahl, die über Wochen oder Tage
entscheidet** — Strommessgerät in Reihe zum Akku, ESP32 im Deep Sleep, IO7 LOW.

## Pins, die Verdacht erregt hatten

**IO19 und IO21 sind frei.** Das native USB des ESP32 liegt nicht an der Buchse:

```
IO19:   P32.P$1, U3.IO19, U5.P$5
IO21:   P33.P$1, U3.IO21, U5.P$6
D+:     J2.DP1, J2.DP2, R3.1          R2, R3 = 22 Ω
D-:     J2.DN1, J2.DN2, R2.1
USB_D+: P4.P$1, R3.2, U1.UD+          → CH340
USB_D-: P3.P$1, R2.2, U1.UD-
```

Die USB-C-Buchse `J2` geht über je 22 Ω ausschließlich an den CH340. IO19/IO20 sind am
Modul zwar USB D−/D+, hier aber nur am Header. Das bestätigt, was `material/CLAUDE.md` §4
schon vermutet hatte — jetzt mit Netzplan statt Rückschluss.

**IO3 ist unbeschaltet:** `IO3: P28.P$1, U3.IO3, U5.P$1` — kein Pull-Widerstand. Damit
gilt wörtlich, was das S3-Datenblatt S. 13 §3.3.4 schreibt: *„This pin does not have any
internal pull resistors and the strapping value must be controlled by the external circuit
that cannot be in a high impedance state."* Der Pegel wählt beim Boot nur die
JTAG-Signalquelle (Tabelle 8); solange `EFUSE_DIS_PAD_JTAG` und `EFUSE_DIS_USB_JTAG`
unprogrammiert sind, ist der Effekt „JTAG über USB-Controller" statt „JTAG über
MTDI/MTCK/MTMS/MTDO". Für einen Taster gegen Masse unkritisch — **IO9 ist trotzdem die
ruhigere Wahl**, dann bleibt IO8 für die Akkumessung frei.

## Was der Plan an offenen Punkten erledigt

**Die Tasten haben externe Pull-ups.** Alle fünf, 4,7 kΩ nach 3V3:

| Taste | GPIO | Pull-up | Netz |
|---|---|---|---|
| EXIT | 1 | `R28` | `IO1_EXIT` |
| MENU | 2 | `R27` | `IO2_MENU` |
| Drehschalter hoch | 4 | `R30` | `IO4_DOWN` |
| Drehschalter Druck | 5 | `R31` | `IO5_CONF` |
| Drehschalter runter | 6 | `R29` | `IO6_UP` |

Die Eingänge flattern also **nicht**. Damit ist der offene Punkt in `CLAUDE.md` → *Stand*
beantwortet: Die drei unerklärten `EXIT: umgeblaettert` in `ha_wechsel` kommen nicht vom
offenen Pin. `INPUT_PULLUP` schadet nicht, ändert aber nichts — die Ursache ist Prellen
oder Software (`ha_wechsel` fragt 50-mal je Sekunde ab, ohne Entprellzeit).

Kurios am Rande: Die Netznamen im Plan sind vertauscht — `IO4_DOWN` und `IO6_UP`, während
am Gerät 4 = hoch und 6 = runter verifiziert ist (`board-gpio.yaml`). Am Gerät gemessen
schlägt Netznamen.

**Der Drehschalter ist ein Quadratur-Encoder.** `K5` ist ein `TM_2024A` mit:

```
K5.1 → IO4      K5.2 → IO6      K5.4 → IO5      K5.3, K5.5…K5.8 → GND
```

Zwei Phasen gegen einen gemeinsamen Masseanschluss plus separater Tastkontakt — das ist
die Beschaltung eines Encoders, nicht dreier getrennter Kontakte. `material/CLAUDE.md` §5
führte das bisher als ungeklärt. Auswertbar wäre damit die echte Drehrichtung statt
zweier einzeln gepollter Pins.

**Die E-Paper-Pinbelegung ist jetzt belegt statt abgelesen.** Sie stand bisher nur in
`spi.h`; der Plan bestätigt sie Netz für Netz — und der Silkscreen druckt sie sogar auf
die Platine (`IO48 => LCD_BUSY`, `IO47 => LCD_RES`, `IO46 => LCD_D/C`, `IO45 => LCD_SPI_CS`,
`IO12 => LCD_SPI_CLK`, `IO11 => LCD_SPI_MOSI`).

## Was auch der Schaltplan nicht sagt

- **Kein Ruhestrom, kein Verbrauch** — nirgends eine Stromaufnahme des fertigen Boards.
- **Keine Strombelastbarkeit** des 3V3 am Header.
- **Kein Herstellername bei `U6`**, deshalb bleibt der Ladestrom erschlossen.
- **Keine RTC.** Bestückt sind nur `U1` CH340C, `U2` RY3420, `U3` ESP32-S3-WROOM-1,
  `U4` FPC-Buchse 24-polig/0,5 mm, `U5` Header, `U6` Lader. Kein RTC-Baustein, kein
  Knopfzellenhalter, kein 32,768-kHz-Quarz. Bleibt: interner RTC des ESP32 am RC-Oszillator
  (driftet) oder die Zeit nach jedem Aufwachen aus dem Netz holen.
- **Die Bauart des Panels** — hinter `U4` liegt die FPC, die beiden SSD1683 sitzen auf dem
  Displaymodul selbst und tauchen im Plan nicht als Bauteile auf.
