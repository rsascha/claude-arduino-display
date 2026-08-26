// laschen — zeichnet die drei Bedienelemente der linken Gehaeusekante als
// Laschen auf das Panel: oben EXIT, unten MENU, dazwischen der Drehschalter.
//
// Zwei Regeln machen den Unterschied zwischen "Demo" und "wiederverwendbar":
//
//   1. Diese Ebene legt KEINEN Bildpuffer an und loescht ihn nicht. Sie zeichnet
//      in den Puffer, der gerade aktiv ist (Paint_NewImage() macht die
//      Anwendung). Nur so laesst sich die Leiste ueber ein bestehendes Bild
//      legen, statt es zu ersetzen.
//   2. Sie liest keine globalen Variablen. Alles, was dargestellt wird, kommt
//      als LaschenZustand herein. Wer wissen will, was auf dem Panel steht,
//      muss nur diese struct ansehen.
//
// Von Tasten weiss die Ebene nur, welche gerade gedrueckt ist — nicht, was das
// bedeutet. Die Zuordnung "EXIT macht scharf" ist Sache der Anwendung.

#ifndef LASCHEN_H
#define LASCHEN_H

#include <Arduino.h>
#include "tasten.h"

struct LaschenZustand {
  // Gerade gedrueckte Taste; ihre Lasche wird invertiert. T_KEINE = keine.
  Taste aktiv;

  // Beschriftung der EXIT-Lasche. Genau EIN Zeichen — mehr passt bei
  // Schriftgroesse 24 nicht in die 46 px breite Lasche.
  const char* exitText;

  // Haelt die EXIT-Lasche invertiert, auch ohne Tastendruck. Fuer Zustaende,
  // die stehen bleiben sollen (in bedienleiste.ino das scharfe "R").
  bool exitInvers;
};

// Zeichnet alle drei Laschen in den aktiven Puffer.
void zeichneLaschen(const LaschenZustand& z);

// Breite der Laschen ab dem linken Rand. Die Anwendung braucht sie, um ihren
// eigenen Inhalt daneben zu setzen, statt darunter.
const int LASCHEN_BREITE = 46;

#endif
