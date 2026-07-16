# QUIC DATAGRAM (RFC 9221) per INET 4.6.0

INET è una **dipendenza intatta**: questa feature non modifica INET nel repo, ma vive come
**singola patch** che si applica a un INET 4.6.0 vanilla (quello gestito da `opp_env`).

## File
| File | Cosa |
|---|---|
| `quic-datagram.patch` | Patch **unica e bidirezionale** (`patch -p1`): modulo di trasporto QUIC + app di test + rete di esempio. |
| `setup-inet.sh` | Applica / rimuove la patch e ricorda il comando di build. |
| `quic-datagram-doc.pdf` / `.tex` | Documentazione completa: classi, metodi, meccanismo no-retransmission, terminologia, test. |

## Uso

Da un INET 4.6.0 vanilla:

```sh
# dalla cartella tod-omnet/inet-patches/
./setup-inet.sh apply      # aggiunge i QUIC DATAGRAM
./setup-inet.sh revert     # torna a INET vanilla
```

Dopo apply/revert, **ricompila INET** (workspace opp_env, dalla radice del workspace):

```sh
opp_env run -w . --no-build -c 'make -C inet-4.6.0 MODE=release -j 4'
```

## Cosa aggiunge la patch
- `FRAME_HEADER_TYPE_DATAGRAM` + `DatagramFrameHeader` + classe `QuicDatagramFrame`
- invio/ricezione datagram in `Connection`, `PacketBuilder`, `ConnectionState`/`EstablishedConnectionState`
- API app: `QuicSocket::sendDatagram()` + callback `socketDatagramArrived()` (`QUIC_C_SEND_DATAGRAM`, `QUIC_I_DATAGRAM`)
- negoziazione `max_datagram_frame_size` (transport parameter)
- app di test `QuicDatagramSender` / `QuicDatagramReceiver` + rete `examples/quic/datagram_test/`

## Note
- La patch è per **INET 4.6.0**. Su un'altra versione il contesto potrebbe non combaciare e la patch fallirebbe (andrebbe rigenerata).
- È **bidirezionale**: la stessa patch con `-R` rimuove tutto. Non servono due file.
- Test rapido dopo l'apply+build: `examples/quic/datagram_test/omnetpp.ini` (link con perdita → si vede consegna inaffidabile, niente ritrasmissione, niente head-of-line blocking).
