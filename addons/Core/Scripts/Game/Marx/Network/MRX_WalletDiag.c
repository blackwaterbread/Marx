#ifdef ENABLE_DIAG
// Diag builds only: "Marx > Show wallet" in the diag menu shows the local wallet in a DbgUI window.

modded enum SCR_DebugMenuID
{
	MRX_DEBUGUI_MENU,
	MRX_DEBUGUI_SHOW_WALLET
}

//------------------------------------------------------------------------------------------------
class MRX_WalletDiag
{
	protected static bool s_bRegistered;

	//------------------------------------------------------------------------------------------------
	static void Register()
	{
		if (s_bRegistered)
			return;

		s_bRegistered = true;
		DiagMenu.RegisterMenu(SCR_DebugMenuID.MRX_DEBUGUI_MENU, "Marx", "");
		DiagMenu.RegisterBool(SCR_DebugMenuID.MRX_DEBUGUI_SHOW_WALLET, "", "Show wallet", "Marx");
	}

	//------------------------------------------------------------------------------------------------
	static void Draw()
	{
		if (!DiagMenu.GetBool(SCR_DebugMenuID.MRX_DEBUGUI_SHOW_WALLET))
			return;

		MRX_ClientWallet wallet = MRX_ClientWallet.GetLocal();
		DbgUI.Begin("Marx wallet", 20, 120);
		if (!wallet)
		{
			DbgUI.Text("no local player");
			DbgUI.End();
			return;
		}

		array<string> currencies = {};
		if (wallet.GetCurrencies(currencies) == 0)
			DbgUI.Text("waiting for the server");

		foreach (string currency : currencies)
		{
			int balance;
			wallet.TryGetBalance(currency, balance);
			DbgUI.Text(string.Format("%1: %2", currency, balance));
		}

		DbgUI.End();
	}
}

//------------------------------------------------------------------------------------------------
modded class ArmaReforgerScripted
{
	//------------------------------------------------------------------------------------------------
	override bool OnGameStart()
	{
		bool result = super.OnGameStart();
		MRX_WalletDiag.Register();
		GetCallqueue().CallLater(MRX_WalletDiag.Draw, 0, true);
		return result;
	}
}
#endif
