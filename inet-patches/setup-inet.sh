#!/usr/bin/env bash
#
# Applica / rimuove la feature "QUIC DATAGRAM (RFC 9221)" su INET 4.6.0.
# INET resta una dipendenza intatta: qui si applica (o si toglie) una singola
# patch bidirezionale a un INET vanilla.
#
# Uso:
#   ./setup-inet.sh apply      # aggiunge i QUIC DATAGRAM
#   ./setup-inet.sh revert     # torna a INET vanilla
#   INET_DIR=/percorso/inet ./setup-inet.sh apply    # INET in altra posizione
#
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PATCH="$HERE/quic-datagram.patch"
INET="${INET_DIR:-$HERE/../../inet-4.6.0}"

[ -f "$PATCH" ] || { echo "ERRORE: patch non trovata: $PATCH" >&2; exit 1; }
[ -d "$INET" ]  || { echo "ERRORE: INET non trovato: $INET (usa INET_DIR=...)" >&2; exit 1; }

case "${1:-}" in
  apply)
    echo ">> Applico i QUIC DATAGRAM a $INET ..."
    if ! patch -d "$INET" -p1 --dry-run < "$PATCH" >/dev/null 2>&1; then
      echo "ERRORE: la patch non si applica pulita (INET gia' patchato, o versione diversa?)." >&2
      exit 1
    fi
    patch -d "$INET" -p1 < "$PATCH"
    echo ">> Fatto."
    ;;
  revert)
    echo ">> Rimuovo i QUIC DATAGRAM da $INET ..."
    if ! patch -d "$INET" -p1 -R --dry-run < "$PATCH" >/dev/null 2>&1; then
      echo "ERRORE: la reverse non si applica pulita (INET non patchato?)." >&2
      exit 1
    fi
    patch -d "$INET" -p1 -R < "$PATCH"
    echo ">> INET riportato a vanilla."
    ;;
  *)
    echo "Uso: $0 apply|revert"; exit 1 ;;
esac

echo
echo "Ora ricompila INET (workspace opp_env, dalla radice del workspace):"
echo "  opp_env run -w . --no-build -c 'make -C inet-4.6.0 MODE=release -j 4'"
