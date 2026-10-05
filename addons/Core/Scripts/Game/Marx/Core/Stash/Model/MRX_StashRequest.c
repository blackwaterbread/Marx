enum MRX_EAssetChangeType
{
	//! Adds m_Asset (STASHED or DEPLOYED).
	ADD,
	//! Moves the asset from m_eExpectedState to m_eNewState. A final new state removes the record.
	UPDATE,
	//! Removes the asset, which must be in m_eExpectedState.
	REMOVE
}

//! One change of a stash request.
class MRX_AssetChange : Managed
{
	MRX_EAssetChangeType m_eType;
	string m_sAssetId;
	//! ADD only.
	ref MRX_AssetRecord m_Asset;
	//! UPDATE and REMOVE: the change fails with INVALID_STATE when the asset is in another state.
	MRX_EAssetState m_eExpectedState;
	//! UPDATE only.
	MRX_EAssetState m_eNewState;
	//! UPDATE only: replaces the snapshot when set.
	ref MRX_ItemSnapshot m_Snapshot;
	//! UPDATE to DEPLOYED only.
	string m_sDeploySession;
	//! UPDATE to STASHED only: replaces the placement when m_bSetPlacement is set.
	string m_sPlacement;
	bool m_bSetPlacement;

	//------------------------------------------------------------------------------------------------
	static MRX_AssetChange Add(notnull MRX_AssetRecord asset)
	{
		MRX_AssetChange change = new MRX_AssetChange();
		change.m_eType = MRX_EAssetChangeType.ADD;
		change.m_sAssetId = asset.m_sId;
		change.m_Asset = asset;
		return change;
	}

	//------------------------------------------------------------------------------------------------
	static MRX_AssetChange Update(string assetId, MRX_EAssetState expectedState, MRX_EAssetState newState, MRX_ItemSnapshot snapshot = null, string deploySession = string.Empty)
	{
		MRX_AssetChange change = new MRX_AssetChange();
		change.m_eType = MRX_EAssetChangeType.UPDATE;
		change.m_sAssetId = assetId;
		change.m_eExpectedState = expectedState;
		change.m_eNewState = newState;
		change.m_Snapshot = snapshot;
		change.m_sDeploySession = deploySession;
		return change;
	}

	//------------------------------------------------------------------------------------------------
	//! UPDATE: also sets the placement (see MRX_AssetRecord.m_sPlacement). \return This change.
	MRX_AssetChange WithPlacement(string placement)
	{
		m_sPlacement = placement;
		m_bSetPlacement = true;
		return this;
	}

	//------------------------------------------------------------------------------------------------
	static MRX_AssetChange Remove(string assetId, MRX_EAssetState expectedState)
	{
		MRX_AssetChange change = new MRX_AssetChange();
		change.m_eType = MRX_EAssetChangeType.REMOVE;
		change.m_sAssetId = assetId;
		change.m_eExpectedState = expectedState;
		return change;
	}

	//------------------------------------------------------------------------------------------------
	MRX_AssetChange Copy()
	{
		MRX_AssetChange change = new MRX_AssetChange();
		change.m_eType = m_eType;
		change.m_sAssetId = m_sAssetId;
		if (m_Asset)
			change.m_Asset = m_Asset.Copy();

		change.m_eExpectedState = m_eExpectedState;
		change.m_eNewState = m_eNewState;
		if (m_Snapshot)
			change.m_Snapshot = m_Snapshot.Copy();

		change.m_sDeploySession = m_sDeploySession;
		change.m_sPlacement = m_sPlacement;
		change.m_bSetPlacement = m_bSetPlacement;
		return change;
	}
}

//! Atomic set of changes to one owner's stash, as passed to the storage backend.
class MRX_StashRequest : Managed
{
	string m_sRequestId;
	string m_sOwnerId;
	ref MRX_TxContext m_Context;
	//! Unix time, written to the changed assets.
	int m_iTimestamp;
	ref array<ref MRX_AssetChange> m_aChanges = {};

	//------------------------------------------------------------------------------------------------
	MRX_StashRequest Copy()
	{
		MRX_StashRequest request = new MRX_StashRequest();
		request.m_sRequestId = m_sRequestId;
		request.m_sOwnerId = m_sOwnerId;
		if (m_Context)
			request.m_Context = m_Context.Copy();

		request.m_iTimestamp = m_iTimestamp;
		foreach (MRX_AssetChange change : m_aChanges)
		{
			request.m_aChanges.Insert(change.Copy());
		}

		return request;
	}
}

//! Result of a stash request (API v0).
class MRX_StashResult : Managed
{
	MRX_EStashStatus m_eStatus;
	string m_sRequestId;
	//! On OK: copies of the changed assets after the change (removed ones in their final state).
	ref array<ref MRX_AssetRecord> m_aAssets = {};

	//------------------------------------------------------------------------------------------------
	static MRX_StashResult Create(MRX_EStashStatus status, string requestId = string.Empty)
	{
		MRX_StashResult result = new MRX_StashResult();
		result.m_eStatus = status;
		result.m_sRequestId = requestId;
		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! \return First changed asset, or null.
	MRX_AssetRecord GetAsset()
	{
		if (m_aAssets.IsEmpty())
			return null;

		return m_aAssets[0];
	}
}
