void MRX_ClientBalanceDelegate(string currency, int balance);
typedef func MRX_ClientBalanceDelegate;

//! Balances of the local player as pushed by the server (API v0). Read-only mirror for UI; the server stays authoritative.
//! Owned by the local SCR_PlayerController, so it starts empty in every session.
class MRX_ClientWallet : Managed
{
	protected ref map<string, int> m_mBalances = new map<string, int>();
	protected ref ScriptInvokerBase<MRX_ClientBalanceDelegate> m_OnBalanceChanged;

	//------------------------------------------------------------------------------------------------
	//! \return Wallet of the local player, or null when there is no local player controller (e.g. dedicated server).
	static MRX_ClientWallet GetLocal()
	{
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
			return null;

		return controller.MRX_GetWallet();
	}

	//------------------------------------------------------------------------------------------------
	//! \return False until the server sent this currency.
	bool TryGetBalance(string currency, out int balance)
	{
		return m_mBalances.Find(currency, balance);
	}

	//------------------------------------------------------------------------------------------------
	int GetCurrencies(notnull array<string> outCurrencies)
	{
		outCurrencies.Clear();
		foreach (string currency, int balance : m_mBalances)
		{
			outCurrencies.Insert(currency);
		}

		return outCurrencies.Count();
	}

	//------------------------------------------------------------------------------------------------
	ScriptInvokerBase<MRX_ClientBalanceDelegate> GetOnBalanceChanged()
	{
		if (!m_OnBalanceChanged)
			m_OnBalanceChanged = new ScriptInvokerBase<MRX_ClientBalanceDelegate>();

		return m_OnBalanceChanged;
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, called when the server pushes a balance.
	void ApplyBalance(string currency, int balance)
	{
		m_mBalances.Set(currency, balance);

#ifdef ENABLE_DIAG
		Print(string.Format("[MRX] Client wallet %1 = %2", currency, balance), LogLevel.NORMAL);
#endif

		if (m_OnBalanceChanged)
			m_OnBalanceChanged.Invoke(currency, balance);
	}
}
