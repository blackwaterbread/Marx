//! Admin wallet command (API v0), from the chat as "#marx ..." (logged-in administrators) or over RCON (admin permission):
//!   marx balance                     (chat only: the executing player's own balance)
//!   marx balance <player> [currency]
//!   marx give <player> <amount> [currency]
//!   marx take <player> <amount> [currency]
//! <player> is a connected player's ID or exact name; names with spaces go in quotes.
//! Changes go through MRX_EconomyService with source LEDGER_SOURCE, like any other balance change.
class MRX_AdminCommand : ScrServerCommand
{
	static const string KEYWORD = "marx";
	static const string LEDGER_SOURCE = "marx_admin";
	protected static const int TIMEOUT_MS = 10000;
	//! Up to 999 999 999, so a single command cannot overflow an int balance on its own.
	protected static const int MAX_AMOUNT_DIGITS = 9;

	protected static int s_iRequestCounter;

	protected ref MRX_AdminPending m_Pending;

	//------------------------------------------------------------------------------------------------
	override string GetKeyword()
	{
		return KEYWORD;
	}

	//------------------------------------------------------------------------------------------------
	override bool IsServerSide()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override int RequiredRCONPermission()
	{
		return ERCONPermissions.PERMISSIONS_ADMIN;
	}

	//------------------------------------------------------------------------------------------------
	override int RequiredChatPermission()
	{
		return EPlayerRole.ADMINISTRATOR;
	}

	//------------------------------------------------------------------------------------------------
	override ref ScrServerCmdResult OnChatClientExecution(array<string> argv, int playerId)
	{
		return ScrServerCmdResult(string.Empty, EServerCmdResultType.OK);
	}

	//------------------------------------------------------------------------------------------------
	override ref ScrServerCmdResult OnChatServerExecution(array<string> argv, int playerId)
	{
		return HandleCommand(argv, playerId);
	}

	//------------------------------------------------------------------------------------------------
	override ref ScrServerCmdResult OnRCONExecution(array<string> argv)
	{
		return HandleCommand(argv, 0);
	}

	//------------------------------------------------------------------------------------------------
	override ref ScrServerCmdResult OnUpdate()
	{
		if (!m_Pending)
			return ScrServerCmdResult(string.Empty, EServerCmdResultType.ERR);

		if (!m_Pending.m_bDone)
		{
			if (System.GetTickCount() - m_Pending.m_iStartTick < TIMEOUT_MS)
				return ScrServerCmdResult(string.Empty, EServerCmdResultType.PENDING);

			m_Pending = null;
			return ScrServerCmdResult("Timed out. A balance change may still complete; check the balance before retrying.", EServerCmdResultType.ERR);
		}

		MRX_AdminPending pending = m_Pending;
		m_Pending = null;
		if (pending.m_eStatus != MRX_ETxStatus.OK)
			return ScrServerCmdResult(string.Format("Failed: %1", typename.EnumToString(MRX_ETxStatus, pending.m_eStatus)), EServerCmdResultType.ERR);

		if (pending.m_bQuery)
			return ScrServerCmdResult(string.Format("%1: %2 %3", pending.m_sTarget, pending.m_iBalance, pending.m_sCurrency), EServerCmdResultType.OK);

		return ScrServerCmdResult(string.Format("%1: %2 %3 %4 (tx %5)", pending.m_sTarget, pending.m_sAction, pending.m_iAmount, pending.m_sCurrency, pending.m_sTxId), EServerCmdResultType.OK);
	}

	//------------------------------------------------------------------------------------------------
	protected ScrServerCmdResult HandleCommand(array<string> argv, int executorId)
	{
		// "#marx balance" without a player: the executing player's own balance (chat only).
		bool ownBalance = argv.Count() == 2 && argv[1] == "balance" && executorId > 0;
		if (argv.Count() < 3 && !ownBalance)
			return GetHelp();

		MRX_EconomyService economy = MRX_Marx.GetEconomy();
		MRX_IdentityService identity = MRX_Marx.GetIdentity();
		if (!economy || !identity)
			return ScrServerCmdResult("Marx is not running on this server", EServerCmdResultType.ERR);

		string action = argv[1];
		bool query = action == "balance";
		if (!query && action != "give" && action != "take")
			return GetHelp();

		// Amount and currency follow the player argument.
		array<string> args = {};
		int targetId = executorId;
		if (!ownBalance)
			targetId = ParsePlayer(argv, 2, args);

		if (targetId <= 0)
			return ScrServerCmdResult("Player not found. Use a connected player's ID or exact name.", EServerCmdResultType.ERR);

		string ownerId = identity.GetOwnerId(targetId);
		if (ownerId.IsEmpty())
			return ScrServerCmdResult("The player's identity is not resolved yet", EServerCmdResultType.ERR);

		int amount;
		int currencyIndex = 0;
		if (!query)
		{
			if (args.IsEmpty())
				return GetHelp();

			amount = ParseAmount(args[0]);
			if (amount <= 0)
				return ScrServerCmdResult(string.Format("Invalid amount '%1'. Use a whole number from 1 to 999999999.", args[0]), EServerCmdResultType.PARAMETERS);

			currencyIndex = 1;
		}

		string currency;
		if (args.Count() > currencyIndex)
			currency = args[currencyIndex];

		currency = ResolveCurrency(economy, currency);
		if (currency.IsEmpty())
			return ScrServerCmdResult("Unknown currency. Known: " + GetCurrencyList(economy), EServerCmdResultType.PARAMETERS);

		MRX_AdminPending pending = new MRX_AdminPending();
		pending.m_iStartTick = System.GetTickCount();
		pending.m_bQuery = query;
		pending.m_sAction = action;
		pending.m_sTarget = string.Format("%1 [%2]", GetGame().GetPlayerManager().GetPlayerName(targetId), targetId);
		pending.m_sCurrency = currency;
		pending.m_iAmount = amount;
		m_Pending = pending;

		if (query)
		{
			economy.GetBalance(ownerId, currency, new MRX_AdminBalanceCallback(pending));
			return ScrServerCmdResult(string.Empty, EServerCmdResultType.PENDING);
		}

		MRX_TxContext context = MRX_TxContext.Create(LEDGER_SOURCE, string.Format("%1 by %2", action, GetExecutorName(executorId)), CreateRequestKey(action));
		MRX_AdminTxCallback callback = new MRX_AdminTxCallback(pending);
		if (action == "give")
			economy.Credit(ownerId, currency, amount, context, callback);
		else
			economy.Debit(ownerId, currency, amount, context, callback);

		return ScrServerCmdResult(string.Empty, EServerCmdResultType.PENDING);
	}

	//------------------------------------------------------------------------------------------------
	protected ScrServerCmdResult GetHelp()
	{
		return ScrServerCmdResult("Marx wallet commands:\n#marx balance (in the chat: your own balance)\n#marx balance <player> [currency]\n#marx give <player> <amount> [currency]\n#marx take <player> <amount> [currency]\n<player> = player ID or exact name (quoted if it has spaces)", EServerCmdResultType.OK);
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the player argument at startIndex, joining a quoted name split by spaces.
	//! \param[out] outRest Arguments after the player argument.
	//! \return Player ID of a connected player, or 0.
	protected int ParsePlayer(array<string> argv, int startIndex, notnull array<string> outRest)
	{
		string player = argv[startIndex];
		int next = startIndex + 1;
		if (player.StartsWith("\""))
		{
			int index = startIndex;
			while (!(player.Length() > 1 && player.EndsWith("\"")) && index + 1 < argv.Count())
			{
				index++;
				player += " " + argv[index];
			}

			if (player.Length() < 2 || !player.EndsWith("\""))
				return 0;

			player = player.Substring(1, player.Length() - 2);
			next = index + 1;
		}

		for (int i = next, count = argv.Count(); i < count; i++)
		{
			outRest.Insert(argv[i]);
		}

		PlayerManager playerManager = GetGame().GetPlayerManager();
		array<int> playerIds = {};
		playerManager.GetPlayers(playerIds);
		if (SCR_StringHelper.IsFormat(SCR_EStringFormat.DIGITS_ONLY, player))
		{
			int playerId = player.ToInt();
			if (playerIds.Contains(playerId))
				return playerId;
		}

		foreach (int playerId : playerIds)
		{
			if (playerManager.GetPlayerName(playerId) == player)
				return playerId;
		}

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Amount, or 0 when the text is not a whole number in range.
	protected static int ParseAmount(string text)
	{
		if (text.IsEmpty() || text.Length() > MAX_AMOUNT_DIGITS || !SCR_StringHelper.IsFormat(SCR_EStringFormat.DIGITS_ONLY, text))
			return 0;

		return text.ToInt();
	}

	//------------------------------------------------------------------------------------------------
	//! \return The currency to use, or empty when it is unknown. Without a currency argument: the default currency
	//! when it exists, otherwise the only configured currency.
	protected static string ResolveCurrency(notnull MRX_EconomyService economy, string currency)
	{
		MRX_CurrencyRegistry currencies = economy.GetRules().m_Currencies;
		if (!currency.IsEmpty())
		{
			if (currencies.Find(currency))
				return currency;

			return string.Empty;
		}

		if (currencies.Find(MRX_Settings.DEFAULT_CURRENCY))
			return MRX_Settings.DEFAULT_CURRENCY;

		array<string> ids = {};
		if (currencies.GetIds(ids) == 1)
			return ids[0];

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	protected static string GetCurrencyList(notnull MRX_EconomyService economy)
	{
		array<string> ids = {};
		economy.GetRules().m_Currencies.GetIds(ids);
		return SCR_StringHelper.Join(", ", ids);
	}

	//------------------------------------------------------------------------------------------------
	protected static string GetExecutorName(int executorId)
	{
		if (executorId <= 0)
			return "rcon";

		return string.Format("%1 [%2]", GetGame().GetPlayerManager().GetPlayerName(executorId), executorId);
	}

	//------------------------------------------------------------------------------------------------
	//! Every admin command is a new request: a repeated command changes the balance again.
	protected static string CreateRequestKey(string action)
	{
		s_iRequestCounter++;
		return string.Format("%1:%2:%3", action, System.GetUnixTime(), s_iRequestCounter);
	}
}

//------------------------------------------------------------------------------------------------
//! State of the running admin command, written by the economy callbacks and polled by OnUpdate. Internal.
class MRX_AdminPending : Managed
{
	bool m_bDone;
	bool m_bQuery;
	int m_iStartTick;
	string m_sAction;
	string m_sTarget;
	string m_sCurrency;
	int m_iAmount;
	MRX_ETxStatus m_eStatus;
	int m_iBalance;
	string m_sTxId;
}

//------------------------------------------------------------------------------------------------
class MRX_AdminTxCallback : MRX_TxCallback
{
	protected ref MRX_AdminPending m_Pending;

	//------------------------------------------------------------------------------------------------
	void MRX_AdminTxCallback(MRX_AdminPending pending)
	{
		m_Pending = pending;
	}

	//------------------------------------------------------------------------------------------------
	override void OnResult(MRX_TxResult result)
	{
		super.OnResult(result);
		m_Pending.m_eStatus = result.m_eStatus;
		m_Pending.m_sTxId = result.m_sTxId;
		m_Pending.m_bDone = true;
		PrintFormat("[MRX] Admin %1 %2 %3 for %4: %5 (tx %6)", m_Pending.m_sAction, m_Pending.m_iAmount, m_Pending.m_sCurrency, m_Pending.m_sTarget, typename.EnumToString(MRX_ETxStatus, result.m_eStatus), result.m_sTxId, level: LogLevel.NORMAL);
	}
}

//------------------------------------------------------------------------------------------------
class MRX_AdminBalanceCallback : MRX_BalanceCallback
{
	protected ref MRX_AdminPending m_Pending;

	//------------------------------------------------------------------------------------------------
	void MRX_AdminBalanceCallback(MRX_AdminPending pending)
	{
		m_Pending = pending;
	}

	//------------------------------------------------------------------------------------------------
	override void OnResult(MRX_ETxStatus status, int balance)
	{
		super.OnResult(status, balance);
		m_Pending.m_eStatus = status;
		m_Pending.m_iBalance = balance;
		m_Pending.m_bDone = true;
	}
}
