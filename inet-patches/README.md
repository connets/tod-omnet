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

### Politica di invio dei datagram
Due scelte, entrambe necessarie perché una sorgente datagram satura non distrugga la connessione
(vedi `[Config Overload*]` in `examples/quic/datagram_test/omnetpp.ini`):

1. **Gli STREAM hanno priorità stretta sui DATAGRAM.** In `PacketBuilder::buildPacket` i DATAGRAM
   frame vengono impacchettati per ultimi, nello spazio che avanza dopo control e STREAM. Nell'ordine
   opposto un frammento datagram da ~1 kB riempie il pacchetto e affama a tempo indeterminato lo
   stream affidabile: nel TOD il canale di controllo (status + istruzioni su stream 0) muore proprio
   quando la rete è congestionata.
2. **La coda datagram è limitata e scarta i più vecchi** (`maxDatagramQueueLength`, default 64,
   0 = illimitata). I DATAGRAM non sono ritrasmessi e trasportano dati con una scadenza: con una coda
   illimitata, un'app che offre più di quanto il path può portare accumula un backlog infinito e
   finisce per spedire solo dati obsoleti, che il ricevitore scarta come fuori deadline.
   Scartando il più vecchio, sul link viaggia sempre il dato più fresco.

   Misurato su `[Config Overload]` (sorgente 17× la capacità del link): a parità di datagram
   consegnati (291), con coda illimitata l'ultimo dato consegnato è vecchio di **9,3 s**, con la coda
   limitata di **0,56 s**.

Statistiche nuove (per connessione): `datagramQueueLength` (backlog dopo ogni tentativo di invio) e
`datagramDropped` (datagram scartati per coda piena).

## Note
- La patch è per **INET 4.6.0**. Su un'altra versione il contesto potrebbe non combaciare e la patch fallirebbe (andrebbe rigenerata).
- È **bidirezionale**: la stessa patch con `-R` rimuove tutto. Non servono due file.
- Test rapido dopo l'apply+build: `examples/quic/datagram_test/omnetpp.ini` (link con perdita → si vede consegna inaffidabile, niente ritrasmissione, niente head-of-line blocking).
