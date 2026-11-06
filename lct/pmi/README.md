# LCT PMI backends

LCT uses PMI-style bootstrap backends to discover rank metadata, exchange
endpoint names, and provide initialization barriers. The preferred production
backends are the launcher's native PMI services (`pmi1`, `pmi2`, or `pmix`) when
they are available.

## TCP backend

The TCP backend is an LCT/LCI-owned bootstrap backend for launch environments
that provide rank information but no PMI/PMIx service. It is independent of
`torchrun`: torchrun is only one launcher that can supply some of the required
environment variables.

To use this backend, every rank must have these required environment variables:

```bash
export LCT_PMI_BACKEND=tcp
export RANK=<global-rank>
export WORLD_SIZE=<number-of-ranks>
export LCT_MASTER_ADDR=<rank-0-hostname-or-ip>
export LCT_MASTER_PORT=<free-port>
```

The TCP rendezvous endpoint can use any one matched address/port pair, checked
in this order:

1. `LCT_MASTER_ADDR` / `LCT_MASTER_PORT`
2. `LCI_MASTER_ADDR` / `LCI_MASTER_PORT`
3. `MASTER_ADDR` / `MASTER_PORT`

When using torchrun, `RANK`, `WORLD_SIZE`, `LOCAL_RANK`, `LOCAL_WORLD_SIZE`,
`MASTER_ADDR`, and `MASTER_PORT` are normally supplied by torchrun. Because
`MASTER_PORT` may already be used by torchrun's own rendezvous service, prefer
to set `LCI_MASTER_ADDR=$MASTER_ADDR` and a separate `LCI_MASTER_PORT` (or the
`LCT_` equivalents) for LCI.

Optional environment variables:

- `LOCAL_RANK` and `LOCAL_WORLD_SIZE` for local-rank validation/diagnostics.
- `LCT_PMI_TCP_TIMEOUT_SEC` for connect, join, barrier, and finalize timeouts
  (default: 60 seconds).
- `LCT_MASTER_FD` (rank 0 only) for a TCP socket that the caller has already
  bound to the endpoint port. See below.

### Passing a bound socket (`LCT_MASTER_FD`)

Choosing a free port up front and then letting rank 0 bind it leaves a window
in which another process can take the port. A caller that bootstraps LCI from
another runtime (for example, MPI) can avoid this by binding the socket itself
and handing it to rank 0:

1. Rank 0 creates a TCP socket, binds it to port 0 (or any port), optionally
   calls `listen()`, and reads the port with `getsockname()`. It keeps the
   socket open.
2. The caller sends rank 0's address and port to every rank (for example, with
   `MPI_Bcast`) and sets `LCT_MASTER_ADDR`/`LCT_MASTER_PORT` on all ranks as
   usual.
3. Rank 0 also sets `LCT_MASTER_FD=<fd>` before initializing LCT/LCI.

Rank 0 checks that the socket is bound to the endpoint port and then listens on
it. LCT does not close the socket; the caller may close it once initialization
returns. Other ranks ignore `LCT_MASTER_FD`.

Endpoint variables are intentionally not mixed across prefixes. For example,
`LCT_MASTER_ADDR` plus `MASTER_PORT` is rejected rather than treated as a valid
pair. `MASTER_ADDR`/`MASTER_PORT` is only a compatibility fallback; torchrun may
already use `MASTER_PORT` for its own rendezvous service, so prefer the `LCT_` or
`LCI_` pair for LCI.

### Zero-background-thread design

The TCP backend does not create background worker threads on any rank. Its KVS
semantics follow PMI's local-put/fence/local-get model:

1. `publish()` stores the key/value in a local pending map and performs no socket
   I/O.
2. `barrier()` is the fence. Nonzero ranks send pending publishes and their
   barrier arrival to rank 0. Rank 0 runs all rendezvous server progress
   synchronously inside its own `barrier()` call with `poll()`, then sends the
   post-fence KVS snapshot back to the nonzero ranks.
3. `getname()` reads only the local post-barrier KVS snapshot and performs no
   socket I/O.
4. `finalize()` closes the rendezvous sockets synchronously; there are no worker
   joins or detached threads.

This keeps normal LCI application work from contending with TCP rendezvous
threads after bootstrap operations return.
