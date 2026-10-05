//! One owned asset (API v0).
class MRX_AssetRecord : Managed
{
	//! Server-generated UUID.
	string m_sId;
	string m_sOwnerId;
	ResourceName m_sPrefab;
	MRX_EAssetState m_eState;
	//! State to restore on the next deploy. Null: a fresh prefab.
	ref MRX_ItemSnapshot m_Snapshot;
	//! Server session that deployed the asset, empty unless DEPLOYED.
	string m_sDeploySession;
	//! Unix time of the last change.
	int m_iUpdatedAt;
	//! Source of the request that created the asset, e.g. "marx_stash" or a consumer mod.
	string m_sSource;

	//------------------------------------------------------------------------------------------------
	static MRX_AssetRecord Create(string id, ResourceName prefab, MRX_ItemSnapshot snapshot = null)
	{
		MRX_AssetRecord record = new MRX_AssetRecord();
		record.m_sId = id;
		record.m_sPrefab = prefab;
		record.m_eState = MRX_EAssetState.STASHED;
		record.m_Snapshot = snapshot;
		return record;
	}

	//------------------------------------------------------------------------------------------------
	MRX_AssetRecord Copy()
	{
		MRX_AssetRecord record = new MRX_AssetRecord();
		record.m_sId = m_sId;
		record.m_sOwnerId = m_sOwnerId;
		record.m_sPrefab = m_sPrefab;
		record.m_eState = m_eState;
		if (m_Snapshot)
			record.m_Snapshot = m_Snapshot.Copy();

		record.m_sDeploySession = m_sDeploySession;
		record.m_iUpdatedAt = m_iUpdatedAt;
		record.m_sSource = m_sSource;
		return record;
	}
}

//! Stored state of one owner's stash: assets and recent idempotency keys.
class MRX_StashRecord : Managed
{
	string m_sOwnerId;
	//! STASHED and DEPLOYED assets. Final states are removed.
	ref array<ref MRX_AssetRecord> m_aAssets = {};
	//! Oldest first, trimmed to MRX_StorageRules.m_iMaxRecentKeys. The entry's tx ID holds the request ID.
	ref array<ref MRX_IdempotencyEntry> m_aRecentKeys = {};

	//------------------------------------------------------------------------------------------------
	static MRX_StashRecord Create(string ownerId)
	{
		MRX_StashRecord record = new MRX_StashRecord();
		record.m_sOwnerId = ownerId;
		return record;
	}

	//------------------------------------------------------------------------------------------------
	MRX_AssetRecord FindAsset(string assetId)
	{
		foreach (MRX_AssetRecord asset : m_aAssets)
		{
			if (asset.m_sId == assetId)
				return asset;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	int CountInState(MRX_EAssetState state)
	{
		int count;
		foreach (MRX_AssetRecord asset : m_aAssets)
		{
			if (asset.m_eState == state)
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Request ID committed with this source and key, or empty when unknown.
	string FindRequestId(string source, string key)
	{
		foreach (MRX_IdempotencyEntry entry : m_aRecentKeys)
		{
			if (entry.m_sKey == key && entry.m_sSource == source)
				return entry.m_sTxId;
		}

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	MRX_StashRecord Copy()
	{
		MRX_StashRecord record = Create(m_sOwnerId);
		foreach (MRX_AssetRecord asset : m_aAssets)
		{
			record.m_aAssets.Insert(asset.Copy());
		}

		foreach (MRX_IdempotencyEntry key : m_aRecentKeys)
		{
			record.m_aRecentKeys.Insert(MRX_IdempotencyEntry.Create(key.m_sSource, key.m_sKey, key.m_sTxId));
		}

		return record;
	}
}
