#!/usr/bin/env python3
"""Erzeugt zu jedem PDF in diesem Ordner eine Textversion mit Seitenmarken.

Aufruf: `make material-txt` (oder direkt `python3 material/pdf2txt.py`).

Warum nicht einfach `pdftotext`: dessen Seitentrenner ist ein Form Feed (\f),
also unsichtbar. Die Marke `=== Seite N ===` macht daraus etwas, das man greppen
kann — Fundstellen lassen sich so mit derselben Seitenangabe zitieren wie aus
dem PDF:

    awk '/^=== Seite /{s=$3} /Suchbegriff/{print s; exit}' datei.txt

`-layout` erhält die Spalten; ohne das zerfallen die Registertabellen des
SSD1683-Datenblatts in unlesbare Wortfolgen.
"""

import io
import os
import shutil
import subprocess
import sys

HIER = os.path.dirname(os.path.abspath(__file__))


def umwandeln(pdf):
    txt = pdf[:-4] + '.txt'
    subprocess.run(['pdftotext', '-layout', pdf, txt], check=True)

    seiten = io.open(txt, encoding='utf-8').read().split('\f')
    if seiten and not seiten[-1].strip():
        seiten.pop()

    with io.open(txt, 'w', encoding='utf-8') as f:
        for nr, seite in enumerate(seiten, 1):
            f.write('=== Seite %d ===\n' % nr)
            f.write(seite.rstrip() + '\n\n')

    return txt, len(seiten)


def main():
    if not shutil.which('pdftotext'):
        sys.exit('pdftotext fehlt — installieren mit: brew install poppler')

    pdfs = sorted(f for f in os.listdir(HIER) if f.lower().endswith('.pdf'))
    if not pdfs:
        sys.exit('Keine PDFs in %s' % HIER)

    for pdf in pdfs:
        txt, seiten = umwandeln(os.path.join(HIER, pdf))
        print('%-38s %3d Seiten, %6.1f KB'
              % (os.path.basename(txt), seiten, os.path.getsize(txt) / 1024))


if __name__ == '__main__':
    main()
