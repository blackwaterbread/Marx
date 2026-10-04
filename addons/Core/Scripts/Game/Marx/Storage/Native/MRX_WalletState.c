//! Persistent scripted state of one wallet (Native backend). Non-singleton: identified by an ID derived from the owner ID.
class MRX_WalletState : Managed
{
	ref MRX_WalletRecord m_Record;
}

//! Must be registered for MRX_WalletState in the persistence config (StatePersistenceConfig in the Marx wallet collection).
class MRX_WalletStateSerializer : ScriptedStateSerializer
{
	static const int VERSION = 1;

	//------------------------------------------------------------------------------------------------
	override static typename GetTargetType()
	{
		return MRX_WalletState;
	}

	//------------------------------------------------------------------------------------------------
	override protected ESerializeResult Serialize(notnull Managed instance, notnull SaveContext context)
	{
		MRX_WalletState state = MRX_WalletState.Cast(instance);
		MRX_WalletRecord record = state.m_Record;
		if (!record)
			return ESerializeResult.ERROR;

		context.WriteValue("version", VERSION);
		context.WriteValue("owner", record.m_sOwnerId);
		context.WriteValue("balances", record.m_mBalances);

		bool typeDiscriminator = context.EnableTypeDiscriminator(false);
		context.WriteValue("entries", record.m_aRecentEntries);
		context.WriteValue("keys", record.m_aRecentKeys);
		context.EnableTypeDiscriminator(typeDiscriminator);

		return ESerializeResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	override protected bool Deserialize(notnull Managed instance, notnull LoadContext context)
	{
		MRX_WalletState state = MRX_WalletState.Cast(instance);
		if (!state.m_Record)
			state.m_Record = new MRX_WalletRecord();

		MRX_WalletRecord record = state.m_Record;

		int version;
		context.ReadValue("version", version);
		if (version > VERSION)
		{
			Print(string.Format("[MRX] Wallet data version %1 is newer than supported version %2", version, VERSION), LogLevel.ERROR);
			return false;
		}

		context.ReadValue("owner", record.m_sOwnerId);
		context.ReadValue("balances", record.m_mBalances);

		bool typeDiscriminator = context.EnableTypeDiscriminator(false);
		context.ReadValue("entries", record.m_aRecentEntries);
		context.ReadValue("keys", record.m_aRecentKeys);
		context.EnableTypeDiscriminator(typeDiscriminator);

		// Missing members come back as null.
		if (!record.m_mBalances)
			record.m_mBalances = new map<string, int>();

		if (!record.m_aRecentEntries)
			record.m_aRecentEntries = {};

		if (!record.m_aRecentKeys)
			record.m_aRecentKeys = {};

		return true;
	}
}
