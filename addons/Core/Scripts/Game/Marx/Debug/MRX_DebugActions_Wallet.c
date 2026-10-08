//! Debug actions of the "Wallet" page. Balance changes go through MRX_EconomyService with source LEDGER_SOURCE, like any
//! other change, so they show in the ledger.

//------------------------------------------------------------------------------------------------
class MRX_DebugWallet
{
	static const string LEDGER_SOURCE = "marx_debug";
	//! The currency argument value for the default currency (MRX_AdminCommand.ResolveCurrency).
	static const string DEFAULT_CURRENCY = "default";
	//! The currency argument value of wallet.history for every currency.
	static const string ALL_CURRENCIES = "all";

	//------------------------------------------------------------------------------------------------
	//! \return The currency to use, or empty when it is unknown.
	static string ResolveCurrency(notnull MRX_EconomyService economy, string value)
	{
		if (value == DEFAULT_CURRENCY)
			value = string.Empty;

		return MRX_AdminCommand.ResolveCurrency(economy, value);
	}

	//------------------------------------------------------------------------------------------------
	static MRX_DebugResult UnknownCurrency(string value)
	{
		return MRX_DebugResult.Create(MRX_EDebugStatus.BAD_ARGS, string.Format("Unknown currency '%1'", value));
	}
}

//------------------------------------------------------------------------------------------------
//! Gives (credits) or takes (debits) money of a player through the ledger.
class MRX_DebugWalletChange : MRX_DebugAction
{
	protected bool m_bTake;

	//------------------------------------------------------------------------------------------------
	override bool HasServerPart()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void RunServer(notnull MRX_DebugContext context, notnull MRX_DebugReply reply)
	{
		MRX_EconomyService economy = MRX_Marx.GetEconomy();
		if (!economy)
		{
			reply.Done(MRX_DebugResult.Failed("Marx is not running on this server"));
			return;
		}

		int amount = MRX_AdminCommand.ParseAmount(context.GetString(0));
		if (amount <= 0)
		{
			reply.Done(MRX_DebugResult.Create(MRX_EDebugStatus.BAD_ARGS, "amount must be a whole number from 1 to 999999999"));
			return;
		}

		string currency = MRX_DebugWallet.ResolveCurrency(economy, context.GetString(1));
		if (currency.IsEmpty())
		{
			reply.Done(MRX_DebugWallet.UnknownCurrency(context.GetString(1)));
			return;
		}

		int targetId = context.GetPlayer(2);
		if (targetId <= 0)
		{
			reply.Done(MRX_DebugResult.Failed("Player not found"));
			return;
		}

		string ownerId = MRX_Marx.GetOwnerId(targetId);
		if (ownerId.IsEmpty())
		{
			reply.Done(MRX_DebugResult.Failed(string.Format("Player %1 has no owner ID yet", targetId)));
			return;
		}

		string sign = "+";
		if (m_bTake)
			sign = "-";

		string change = string.Format("player %1 %2%3 %4", targetId, sign, amount, currency);
		MRX_TxContext txContext = MRX_TxContext.Create(MRX_DebugWallet.LEDGER_SOURCE, GetId(), "debug:" + MRX_Marx.NewId());
		MRX_DebugTxReply callback = new MRX_DebugTxReply(reply, ownerId, change);
		if (m_bTake)
			economy.Debit(ownerId, currency, amount, txContext, callback);
		else
			economy.Credit(ownerId, currency, amount, txContext, callback);
	}

	//------------------------------------------------------------------------------------------------
	protected void SetupChange(string id, string label, bool take)
	{
		Setup(id, "Wallet", label);
		AddArg("amount", "", true);
		AddArg("currency", MRX_DebugWallet.DEFAULT_CURRENCY);
		AddArg("player", MRX_DebugContext.ME);
		m_bTake = take;
	}
}

//------------------------------------------------------------------------------------------------
class MRX_DebugWalletGive : MRX_DebugWalletChange
{
	//------------------------------------------------------------------------------------------------
	void MRX_DebugWalletGive()
	{
		SetupChange("wallet.give", "Give money", false);
	}
}

//------------------------------------------------------------------------------------------------
class MRX_DebugWalletTake : MRX_DebugWalletChange
{
	//------------------------------------------------------------------------------------------------
	void MRX_DebugWalletTake()
	{
		SetupChange("wallet.take", "Take money", true);
	}
}

//------------------------------------------------------------------------------------------------
//! The requesting player's latest ledger entries, newest first.
class MRX_DebugWalletHistory : MRX_DebugAction
{
	//------------------------------------------------------------------------------------------------
	void MRX_DebugWalletHistory()
	{
		Setup("wallet.history", "Wallet", "History");
		AddArg("count", "10", true);
		AddArg("currency", MRX_DebugWallet.DEFAULT_CURRENCY);
	}

	//------------------------------------------------------------------------------------------------
	override bool HasServerPart()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void RunServer(notnull MRX_DebugContext context, notnull MRX_DebugReply reply)
	{
		MRX_EconomyService economy = MRX_Marx.GetEconomy();
		if (!economy)
		{
			reply.Done(MRX_DebugResult.Failed("Marx is not running on this server"));
			return;
		}

		string ownerId = MRX_Marx.GetOwnerId(context.m_iPlayerId);
		if (ownerId.IsEmpty())
		{
			reply.Done(MRX_DebugResult.Failed("You have no owner ID yet"));
			return;
		}

		string currency;
		if (context.GetString(1) != MRX_DebugWallet.ALL_CURRENCIES)
		{
			currency = MRX_DebugWallet.ResolveCurrency(economy, context.GetString(1));
			if (currency.IsEmpty())
			{
				reply.Done(MRX_DebugWallet.UnknownCurrency(context.GetString(1)));
				return;
			}
		}

		economy.GetHistory(ownerId, currency, context.GetInt(0), new MRX_DebugHistoryReply(reply));
	}
}

//------------------------------------------------------------------------------------------------
//! Answers a balance change with the new balance. Internal.
class MRX_DebugTxReply : MRX_TxCallback
{
	protected ref MRX_DebugReply m_Reply;
	protected string m_sOwnerId;
	protected string m_sChange;

	//------------------------------------------------------------------------------------------------
	void MRX_DebugTxReply(MRX_DebugReply reply, string ownerId, string change)
	{
		m_Reply = reply;
		m_sOwnerId = ownerId;
		m_sChange = change;
	}

	//------------------------------------------------------------------------------------------------
	override void OnResult(MRX_TxResult result)
	{
		super.OnResult(result);
		if (result.m_eStatus != MRX_ETxStatus.OK)
		{
			m_Reply.Done(MRX_DebugResult.Failed(string.Format("%1: %2", m_sChange, typename.EnumToString(MRX_ETxStatus, result.m_eStatus))));
			return;
		}

		string balance = "?";
		foreach (MRX_LedgerEntry entry : result.m_aEntries)
		{
			if (entry.m_sOwnerId == m_sOwnerId)
				balance = entry.m_iBalanceAfter.ToString();
		}

		m_Reply.Done(MRX_DebugResult.Ok(string.Format("%1, balance %2 (tx %3)", m_sChange, balance, result.m_sTxId)));
	}
}

//------------------------------------------------------------------------------------------------
//! Answers with the ledger entries, one per line. Internal.
class MRX_DebugHistoryReply : MRX_HistoryCallback
{
	protected ref MRX_DebugReply m_Reply;

	//------------------------------------------------------------------------------------------------
	void MRX_DebugHistoryReply(MRX_DebugReply reply)
	{
		m_Reply = reply;
	}

	//------------------------------------------------------------------------------------------------
	override void OnResult(MRX_ETxStatus status, array<ref MRX_LedgerEntry> entries)
	{
		super.OnResult(status, entries);
		if (status != MRX_ETxStatus.OK)
		{
			m_Reply.Done(MRX_DebugResult.Failed(typename.EnumToString(MRX_ETxStatus, status)));
			return;
		}

		int now = System.GetUnixTime();
		string text = string.Format("%1 entries", entries.Count());
		foreach (MRX_LedgerEntry entry : entries)
		{
			string delta = entry.m_iDelta.ToString();
			if (entry.m_iDelta > 0)
				delta = "+" + delta;

			text += string.Format("\n%1 s ago: %2 %3 -> %4 [%5] %6", now - entry.m_iTimestamp, delta, entry.m_sCurrency, entry.m_iBalanceAfter, entry.m_sSource, entry.m_sReason);
		}

		m_Reply.Done(MRX_DebugResult.Ok(text));
	}
}
