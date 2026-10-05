# Economy API

All calls below run on the server. `MRX_Marx.GetEconomy()` returns null on clients.

## Rules

- Every balance change goes through `MRX_EconomyService` with an `MRX_TxContext`. There is no other way to change a
  balance, and the ledger is the source of truth.
- Amounts are `int`. Currencies come from the settings; each has an initial balance, a maximum and an optional
  negative range.
- Calls on the same owner run one at a time, in call order; a transfer waits for both owners. Calls made before the
  storage is ready wait for it.
- Callbacks always run on a later frame, never inside the call.

## Context and idempotency

```c
MRX_TxContext context = MRX_TxContext.Create("my_mod", "contract_reward", "contract:" + contractId + ":" + ownerId);
```

| Field | Meaning |
|---|---|
| source | Your mod's ID. Idempotency keys are scoped by source. |
| reason | Free text written to the ledger. |
| idempotency key | Required. A second call with the same source and key returns `DUPLICATE` with the original transaction and changes nothing. |

Build the key from what makes the event unique (contract ID, kill ID, ...). Use `MRX_Marx.NewId()` for one-off events
that have no ID of their own. Local backends remember the last 200 keys per wallet (setting); failed calls do not
use up their key, so they can be retried with it.

## Calls

```c
MRX_EconomyService economy = MRX_Marx.GetEconomy();
economy.Credit(ownerId, "cash", 100, context, callback);
economy.Debit(ownerId, "cash", 30, context, callback);
economy.Transfer(fromOwnerId, toOwnerId, "cash", 25, context, callback);
economy.GetBalance(ownerId, "cash", balanceCallback);
economy.GetHistory(ownerId, "cash", 20, historyCallback);    // newest first; empty currency = all
```

Offline owners can be credited and debited. `WatchOwner(ownerId)` keeps an owner's balances cached
(`TryGetCachedBalance`); Marx watches every connected player.

## Callbacks

Script methods cannot take functions as arguments, so results come through callback objects. Either subscribe a
method:

```c
MRX_TxCallback callback = new MRX_TxCallback();
callback.GetOnResult().Insert(OnPaid);   // void OnPaid(MRX_TxResult result)
```

or subclass the callback and override `OnResult`. One callback object can be reused for several calls.

| Callback | Result |
|---|---|
| `MRX_TxCallback` | `MRX_TxResult`: status, transaction ID, ledger entries |
| `MRX_BalanceCallback` | status, balance |
| `MRX_HistoryCallback` | status, `array<ref MRX_LedgerEntry>` |

`MRX_TxResult.IsCommitted()` is true for `OK` and `DUPLICATE`.

## Status codes (`MRX_ETxStatus`)

`OK`, `DUPLICATE`, `INSUFFICIENT_FUNDS`, `INVALID_AMOUNT` (zero, negative, out of range), `UNKNOWN_CURRENCY`,
`LIMIT_EXCEEDED`, `OWNER_NOT_READY` (empty owner), `INVALID_OWNER` (e.g. transfer to self), `INVALID_CONTEXT`
(missing source or key), `REJECTED` (by a validator), `STORAGE_ERROR`, `BUSY`.

A `STORAGE_ERROR` after a commit timeout may still have been committed. Retry with the same context: the retry
returns `DUPLICATE` if the first call went through.

## Events and extension points

| API | When |
|---|---|
| `GetOnTransactionCommitted()` | once per ledger entry (`MRX_LedgerEntry`) |
| `GetOnBalanceChanged()` | `(ownerId, currency, balance)` after every change of a watched owner, and on the first load |
| `AddValidator(MRX_TxValidator)` | override `Validate(MRX_TxRequest)`; return false to refuse with `REJECTED` |

## Client side

The server pushes the balances of each player to that player's client. Read them for UI with
`MRX_ClientWallet.GetLocal()`: `TryGetBalance(currency, out balance)`, `GetCurrencies(out)`, and
`GetOnBalanceChanged()` `(currency, balance)`. It is a read-only mirror; clients never send amounts.
