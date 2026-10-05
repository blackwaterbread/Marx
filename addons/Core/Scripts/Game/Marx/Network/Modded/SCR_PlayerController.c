//! Balance push from the server to the owning client. Only MRX_ prefixed members are added.
modded class SCR_PlayerController
{
	protected ref MRX_ClientWallet m_MRX_Wallet;

	//------------------------------------------------------------------------------------------------
	//! Local balances of this controller's player (client side, also on a listen server host).
	MRX_ClientWallet MRX_GetWallet()
	{
		if (!m_MRX_Wallet)
			m_MRX_Wallet = new MRX_ClientWallet();

		return m_MRX_Wallet;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: sends a balance to the player who owns this controller.
	void MRX_SendBalance(string currency, int balance)
	{
		Rpc(MRX_RpcDo_Balance, currency, balance);
	}

	//------------------------------------------------------------------------------------------------
	//! Runs on the owning client, or directly on the server when the server owns this controller (host).
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void MRX_RpcDo_Balance(string currency, int balance)
	{
		MRX_GetWallet().ApplyBalance(currency, balance);
	}
}
