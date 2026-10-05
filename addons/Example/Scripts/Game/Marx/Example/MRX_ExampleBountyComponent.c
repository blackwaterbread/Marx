[ComponentEditorProps(category: "Marx/Example", description: "Example Marx consumer: pays a bounty to players who kill another player.")]
class MRX_ExampleBountyComponentClass : SCR_BaseGameModeComponentClass
{
}

//! Example consumer of the Marx API (not used by Marx itself). Add it to a game mode prefab or entity.
//! Shows the usual pattern: resolve the owner from the player ID, then change balances only through the economy
//! service with a context (source, reason, idempotency key).
class MRX_ExampleBountyComponent : SCR_BaseGameModeComponent
{
	//! Identifies this mod in the ledger.
	static const string LEDGER_SOURCE = "marx_example";

	[Attribute("10", desc: "Amount paid per kill")]
	protected int m_iBounty;

	[Attribute("cash", desc: "Currency ID from the Marx settings")]
	protected string m_sCurrency;

	//------------------------------------------------------------------------------------------------
	override void OnPlayerKilled(notnull SCR_InstigatorContextData instigatorContextData)
	{
		super.OnPlayerKilled(instigatorContextData);

		// The economy is server-only (MRX_Marx returns null on clients).
		MRX_EconomyService economy = MRX_Marx.GetEconomy();
		if (!economy)
			return;

		int killerId = instigatorContextData.GetKillerPlayerID();
		if (killerId <= 0 || killerId == instigatorContextData.GetVictimPlayerID())
			return;

		// Empty until the player's identity is known; Credit then fails with OWNER_NOT_READY.
		string ownerId = MRX_Marx.GetOwnerId(killerId);
		if (ownerId.IsEmpty())
			return;

		// One key per kill. Retrying a call with the same key (e.g. after a storage timeout) cannot pay twice.
		string key = "kill:" + MRX_Marx.NewId();
		MRX_TxContext context = MRX_TxContext.Create(LEDGER_SOURCE, "bounty", key);

		MRX_TxCallback callback = new MRX_TxCallback();
		callback.GetOnResult().Insert(OnBountyPaid);
		economy.Credit(ownerId, m_sCurrency, m_iBounty, context, callback);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnBountyPaid(MRX_TxResult result)
	{
		if (!result.IsCommitted())
			Print(string.Format("[MRX_EXAMPLE] Bounty not paid: %1", typename.EnumToString(MRX_ETxStatus, result.m_eStatus)), LogLevel.WARNING);
	}
}
