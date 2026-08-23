#!/bin/bash
# Holt echte Antworten aus Home Assistant nach tools/simulator/data/.
#
# Der Simulator liest sie an der Stelle, an der auf dem Geraet die HTTP-Anfrage
# steht. Damit laeuft im Simulator derselbe Parser ueber dieselben Bytes wie auf
# dem ESP32 — Abweichungen im Bild koennen dann nur noch vom Zeichencode kommen.
#
# Aufruf: tools/simulator/fetch.sh sketches/ha_raeume [TAGE]
set -euo pipefail

SKETCH="${1:?Aufruf: fetch.sh sketches/<name> [tage]}"
DAYS="${2:-10}"
DIR="$(cd "$(dirname "$0")" && pwd)/data"
SECRETS="$SKETCH/secrets.h"

[ -f "$SECRETS" ] || { echo "Keine $SECRETS"; exit 1; }
val() { grep "^#define $1" "$SECRETS" | sed 's/.*"\(.*\)".*/\1/'; }
HOST=$(val HA_HOST); TOKEN=$(val HA_TOKEN)
PORT=$(grep '^#define HA_PORT' "$SECRETS" | awk '{print $3}')

# entity_ids aus dem Sketch ziehen — keine zweite Liste, die veralten kann.
# Auch weather.*: ha_wetter liest den Zustand von weather.forecast_home.
#
# #include-Zeilen muessen vorher raus: '#include "weather.h"' sieht fuer das
# Muster aus wie die entity_id 'weather.h', und HA antwortet darauf mit 404 —
# was wie ein kaputter Sensor aussieht, aber ein Dateiname ist.
ENTITIES=$(cat "$SKETCH"/*.ino | grep -v '^#include' \
           | grep -oE '"(sensor|weather)\.[a-z0-9_]+"' | sed 's/.*"\(.*\)"/\1/' | sort -u)
[ -n "$ENTITIES" ] || { echo "Keine entity_ids in $SKETCH gefunden"; exit 1; }

mkdir -p "$DIR"
S=$(python3 -c "import datetime;print((datetime.datetime.now(datetime.UTC)-datetime.timedelta(days=$DAYS)).strftime('%Y-%m-%dT%H:%M:%SZ'))")
E=$(python3 -c "import datetime;print(datetime.datetime.now(datetime.UTC).strftime('%Y-%m-%dT%H:%M:%SZ'))")

for e in $ENTITIES; do
  # Zeitstempel als Z, nicht +00:00: das '+' wird im Query-String zum
  # Leerzeichen dekodiert, HA antwortet dann mit "Invalid end_time".
  curl -sf -H "Authorization: Bearer $TOKEN" \
    "http://$HOST:$PORT/api/history/period/$S?filter_entity_id=$e&end_time=$E&minimal_response&no_attributes" \
    -o "$DIR/$e.json"
  curl -sf -H "Authorization: Bearer $TOKEN" \
    "http://$HOST:$PORT/api/states/$e" -o "$DIR/state_$e.json"
  printf '  %-52s %6s Byte\n' "$e" "$(wc -c < "$DIR/$e.json" | tr -d ' ')"
done
echo "Daten in $DIR ($DAYS Tage)"
