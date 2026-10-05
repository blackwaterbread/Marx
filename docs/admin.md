# Admin commands

`MRX_AdminCommand` adds the `marx` server command:

```
#marx balance
#marx balance <player> [currency]
#marx give <player> <amount> [currency]
#marx take <player> <amount> [currency]
```

- In the chat, for logged-in administrators (`#login`). Over RCON, with admin permission.
- `#marx balance` without a player shows your own balance (chat only).
- `<player>` is a connected player's ID or exact name; put names with spaces in quotes.
- `<amount>` is a whole number from 1 to 999999999.
- Without a currency, `cash` is used if it exists, otherwise the only configured currency.
- Changes go through the economy service like any other change, with ledger source `marx_admin` and the executing
  admin in the reason. Every command is a new transaction.
- The answer comes when the storage confirms; after 10 s without an answer the command reports a timeout (the change
  may still complete, so check the balance before repeating it).
