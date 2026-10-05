//! Persistent scripted state of one stash (Native backend). Non-singleton: identified by an ID derived from the owner ID.
class MRX_StashState : Managed
{
	ref MRX_StashRecord m_Record;
}

//! Must be registered for MRX_StashState in the persistence config (StatePersistenceConfig in the Marx stash collection).
class MRX_StashStateSerializer : ScriptedStateSerializer
{
	static const int VERSION = 1;

	//------------------------------------------------------------------------------------------------
	override static typename GetTargetType()
	{
		return MRX_StashState;
	}

	//------------------------------------------------------------------------------------------------
	override protected ESerializeResult Serialize(notnull Managed instance, notnull SaveContext context)
	{
		MRX_StashState state = MRX_StashState.Cast(instance);
		MRX_StashRecord record = state.m_Record;
		if (!record)
			return ESerializeResult.ERROR;

		context.WriteValue("version", VERSION);
		context.WriteValue("owner", record.m_sOwnerId);

		bool typeDiscriminator = context.EnableTypeDiscriminator(false);
		context.WriteValue("assets", record.m_aAssets);
		context.WriteValue("keys", record.m_aRecentKeys);
		context.EnableTypeDiscriminator(typeDiscriminator);

		return ESerializeResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	override protected bool Deserialize(notnull Managed instance, notnull LoadContext context)
	{
		MRX_StashState state = MRX_StashState.Cast(instance);
		if (!state.m_Record)
			state.m_Record = new MRX_StashRecord();

		MRX_StashRecord record = state.m_Record;

		int version;
		context.ReadValue("version", version);
		if (version > VERSION)
		{
			Print(string.Format("[MRX] Stash data version %1 is newer than supported version %2", version, VERSION), LogLevel.ERROR);
			return false;
		}

		context.ReadValue("owner", record.m_sOwnerId);

		bool typeDiscriminator = context.EnableTypeDiscriminator(false);
		context.ReadValue("assets", record.m_aAssets);
		context.ReadValue("keys", record.m_aRecentKeys);
		context.EnableTypeDiscriminator(typeDiscriminator);

		// Missing members come back as null.
		if (!record.m_aAssets)
			record.m_aAssets = {};

		if (!record.m_aRecentKeys)
			record.m_aRecentKeys = {};

		foreach (MRX_AssetRecord asset : record.m_aAssets)
		{
			if (asset.m_Snapshot)
				RepairSnapshot(asset.m_Snapshot);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void RepairSnapshot(notnull MRX_ItemSnapshot snapshot)
	{
		if (!snapshot.m_aHitZones)
			snapshot.m_aHitZones = {};

		if (!snapshot.m_aFuel)
			snapshot.m_aFuel = {};

		if (!snapshot.m_aChildren)
			snapshot.m_aChildren = {};

		foreach (MRX_ItemSnapshot child : snapshot.m_aChildren)
		{
			RepairSnapshot(child);
		}
	}
}
