//! Stash rules for backends that keep stash records themselves (InMemory, Native). No engine dependencies.
class MRX_StashMath
{
	//------------------------------------------------------------------------------------------------
	//! Applies all changes or none. A committed (source, key) returns DUPLICATE; failed requests do not consume their key.
	//! \param record Stash of the request owner. Modified only on OK.
	static MRX_StashResult Apply(notnull MRX_StashRecord record, notnull MRX_StashRequest request, notnull MRX_StorageRules rules)
	{
		MRX_TxContext context = request.m_Context;
		if (!context || !context.IsValid())
			return MRX_StashResult.Create(MRX_EStashStatus.INVALID_CONTEXT, request.m_sRequestId);

		if (request.m_sOwnerId.IsEmpty())
			return MRX_StashResult.Create(MRX_EStashStatus.OWNER_NOT_READY, request.m_sRequestId);

		if (request.m_aChanges.IsEmpty() && request.m_aPropertyChanges.IsEmpty())
			return MRX_StashResult.Create(MRX_EStashStatus.INVALID_STATE, request.m_sRequestId);

		string committedId = record.FindRequestId(context.m_sSource, context.m_sIdempotencyKey);
		if (!committedId.IsEmpty())
			return MRX_StashResult.Create(MRX_EStashStatus.DUPLICATE, committedId);

		MRX_StashRecord working = record.Copy();
		int countBefore = working.m_aAssets.Count();
		MRX_StashResult result = MRX_StashResult.Create(MRX_EStashStatus.OK, request.m_sRequestId);
		foreach (MRX_AssetChange change : request.m_aChanges)
		{
			MRX_EStashStatus status = ApplyChange(working, change, request, result);
			if (status != MRX_EStashStatus.OK)
				return MRX_StashResult.Create(status, request.m_sRequestId);
		}

		foreach (MRX_PropertyChange propertyChange : request.m_aPropertyChanges)
		{
			if (propertyChange.m_sKey.IsEmpty())
				return MRX_StashResult.Create(MRX_EStashStatus.INVALID_STATE, request.m_sRequestId);

			if (propertyChange.m_bCheck && working.GetProperty(propertyChange.m_sKey) != propertyChange.m_sExpected)
				return MRX_StashResult.Create(MRX_EStashStatus.INVALID_STATE, request.m_sRequestId);

			working.SetProperty(propertyChange.m_sKey, propertyChange.m_sValue);
		}

		// A stash above the limit (e.g. after a config change) still accepts changes that do not add assets.
		int countAfter = working.m_aAssets.Count();
		if (rules.m_iMaxStashAssets > 0 && countAfter > rules.m_iMaxStashAssets && countAfter > countBefore)
			return MRX_StashResult.Create(MRX_EStashStatus.STASH_FULL, request.m_sRequestId);

		record.m_aAssets = working.m_aAssets;
		record.m_aProperties = working.m_aProperties;
		record.m_aRecentKeys.Insert(MRX_IdempotencyEntry.Create(context.m_sSource, context.m_sIdempotencyKey, request.m_sRequestId));
		while (record.m_aRecentKeys.Count() > rules.m_iMaxRecentKeys)
		{
			record.m_aRecentKeys.RemoveOrdered(0);
		}

		return result;
	}

	//------------------------------------------------------------------------------------------------
	protected static MRX_EStashStatus ApplyChange(notnull MRX_StashRecord working, notnull MRX_AssetChange change, notnull MRX_StashRequest request, notnull MRX_StashResult result)
	{
		if (change.m_eType == MRX_EAssetChangeType.ADD)
			return ApplyAdd(working, change, request, result);

		MRX_AssetRecord asset = working.FindAsset(change.m_sAssetId);
		if (!asset)
			return MRX_EStashStatus.UNKNOWN_ASSET;

		if (asset.m_eState != change.m_eExpectedState)
			return MRX_EStashStatus.INVALID_STATE;

		if (change.m_eType == MRX_EAssetChangeType.REMOVE)
		{
			result.m_aAssets.Insert(asset.Copy());
			working.m_aAssets.RemoveItemOrdered(asset);
			return MRX_EStashStatus.OK;
		}

		if (!MRX_AssetStates.CanTransition(asset.m_eState, change.m_eNewState))
			return MRX_EStashStatus.INVALID_STATE;

		asset.m_eState = change.m_eNewState;
		asset.m_iUpdatedAt = request.m_iTimestamp;
		if (change.m_Snapshot)
			asset.m_Snapshot = change.m_Snapshot.Copy();

		if (asset.m_eState == MRX_EAssetState.DEPLOYED)
			asset.m_sDeploySession = change.m_sDeploySession;
		else
			asset.m_sDeploySession = string.Empty;

		// A placement only means something inside the stash.
		if (asset.m_eState != MRX_EAssetState.STASHED)
			asset.m_sPlacement = string.Empty;
		else if (change.m_bSetPlacement)
			asset.m_sPlacement = change.m_sPlacement;

		result.m_aAssets.Insert(asset.Copy());
		if (MRX_AssetStates.IsFinal(asset.m_eState))
			working.m_aAssets.RemoveItemOrdered(asset);

		return MRX_EStashStatus.OK;
	}

	//------------------------------------------------------------------------------------------------
	protected static MRX_EStashStatus ApplyAdd(notnull MRX_StashRecord working, notnull MRX_AssetChange change, notnull MRX_StashRequest request, notnull MRX_StashResult result)
	{
		MRX_AssetRecord added = change.m_Asset;
		if (!added || added.m_sId.IsEmpty() || MRX_AssetStates.IsFinal(added.m_eState))
			return MRX_EStashStatus.INVALID_STATE;

		if (added.m_sPrefab.IsEmpty())
			return MRX_EStashStatus.INVALID_PREFAB;

		if (working.FindAsset(added.m_sId))
			return MRX_EStashStatus.ASSET_EXISTS;

		MRX_AssetRecord asset = added.Copy();
		asset.m_sOwnerId = request.m_sOwnerId;
		asset.m_iUpdatedAt = request.m_iTimestamp;
		if (asset.m_sSource.IsEmpty())
			asset.m_sSource = request.m_Context.m_sSource;

		if (asset.m_eState != MRX_EAssetState.DEPLOYED)
			asset.m_sDeploySession = string.Empty;

		if (asset.m_eState != MRX_EAssetState.STASHED)
			asset.m_sPlacement = string.Empty;

		working.m_aAssets.Insert(asset);
		result.m_aAssets.Insert(asset.Copy());
		return MRX_EStashStatus.OK;
	}
}
