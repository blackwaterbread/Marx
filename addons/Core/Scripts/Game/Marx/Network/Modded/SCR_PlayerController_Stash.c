void MRX_ClientStashListDelegate(MRX_EStashStatus status, array<string> assetIds, array<string> prefabs, array<int> states);
typedef func MRX_ClientStashListDelegate;

void MRX_ClientStashResultDelegate(MRX_EStashStatus status, string assetId);
typedef func MRX_ClientStashResultDelegate;

//! Stash requests at a stash point (API v0): the client sends IDs only, the server checks the stash point distance and
//! ownership, and answers with the result (and the stash content on request).
modded class SCR_PlayerController
{
	protected static const int MRX_STASH_REQUEST_INTERVAL_MS = 250;

	protected int m_iMRX_LastStashRequestTick;
	protected ref ScriptInvokerBase<MRX_ClientStashListDelegate> m_MRX_OnStashList;
	protected ref ScriptInvokerBase<MRX_ClientStashResultDelegate> m_MRX_OnStashResult;

	//------------------------------------------------------------------------------------------------
	//! Client: invoked with the stash content (parallel arrays; states are MRX_EAssetState values).
	ScriptInvokerBase<MRX_ClientStashListDelegate> MRX_GetOnStashList()
	{
		if (!m_MRX_OnStashList)
			m_MRX_OnStashList = new ScriptInvokerBase<MRX_ClientStashListDelegate>();

		return m_MRX_OnStashList;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: invoked with the server's answer to a deposit or withdraw request.
	ScriptInvokerBase<MRX_ClientStashResultDelegate> MRX_GetOnStashResult()
	{
		if (!m_MRX_OnStashResult)
			m_MRX_OnStashResult = new ScriptInvokerBase<MRX_ClientStashResultDelegate>();

		return m_MRX_OnStashResult;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: asks for the stash content.
	void MRX_RequestStashList(notnull IEntity stashPoint)
	{
		Rpc(MRX_RpcAsk_StashList, SCR_EntityHelper.EntityToRplId(stashPoint));
	}

	//------------------------------------------------------------------------------------------------
	//! Client: asks to store an item of the player's inventory.
	void MRX_RequestStashDeposit(notnull IEntity stashPoint, notnull IEntity item)
	{
		InventoryItemComponent itemComponent = InventoryItemComponent.Cast(item.FindComponent(InventoryItemComponent));
		if (!itemComponent)
			return;

		Rpc(MRX_RpcAsk_StashDeposit, SCR_EntityHelper.EntityToRplId(stashPoint), Replication.FindItemId(itemComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! Client: asks to take a stashed asset into the player's inventory.
	void MRX_RequestStashWithdraw(notnull IEntity stashPoint, string assetId)
	{
		Rpc(MRX_RpcAsk_StashWithdraw, SCR_EntityHelper.EntityToRplId(stashPoint), assetId);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void MRX_RpcAsk_StashList(RplId stashPointId)
	{
		MRX_EStashStatus status = MRX_CheckStashRequest(stashPointId);
		if (status != MRX_EStashStatus.OK)
		{
			Rpc(MRX_RpcDo_StashList, status, new array<string>(), new array<string>(), new array<int>());
			return;
		}

		MRX_Marx.GetStash().List(MRX_Marx.GetOwnerId(GetPlayerId()), new MRX_StashListReply(this));
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void MRX_RpcAsk_StashDeposit(RplId stashPointId, RplId itemId)
	{
		MRX_EStashStatus status = MRX_CheckStashRequest(stashPointId);
		if (status != MRX_EStashStatus.OK)
		{
			MRX_SendStashResult(status, string.Empty);
			return;
		}

		IEntity item;
		InventoryItemComponent itemComponent = InventoryItemComponent.Cast(Replication.FindItem(itemId));
		if (itemComponent)
			item = itemComponent.GetOwner();

		MRX_Marx.GetStash().Deposit(GetPlayerId(), item, new MRX_StashRpcReply(this));
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void MRX_RpcAsk_StashWithdraw(RplId stashPointId, string assetId)
	{
		MRX_EStashStatus status = MRX_CheckStashRequest(stashPointId);
		if (status != MRX_EStashStatus.OK)
		{
			MRX_SendStashResult(status, assetId);
			return;
		}

		MRX_Marx.GetStash().Withdraw(GetPlayerId(), assetId, new MRX_StashRpcReply(this));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: sends the stash content to the player who owns this controller.
	void MRX_SendStashList(MRX_EStashStatus status, MRX_StashRecord record)
	{
		array<string> assetIds = {};
		array<string> prefabs = {};
		array<int> states = {};
		if (record)
		{
			foreach (MRX_AssetRecord asset : record.m_aAssets)
			{
				assetIds.Insert(asset.m_sId);
				prefabs.Insert(asset.m_sPrefab);
				states.Insert(asset.m_eState);
			}
		}

		Rpc(MRX_RpcDo_StashList, status, assetIds, prefabs, states);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: sends a deposit or withdraw result to the player who owns this controller.
	void MRX_SendStashResult(MRX_EStashStatus status, string assetId)
	{
		Rpc(MRX_RpcDo_StashResult, status, assetId);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void MRX_RpcDo_StashList(MRX_EStashStatus status, array<string> assetIds, array<string> prefabs, array<int> states)
	{
		if (m_MRX_OnStashList)
			m_MRX_OnStashList.Invoke(status, assetIds, prefabs, states);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void MRX_RpcDo_StashResult(MRX_EStashStatus status, string assetId)
	{
		if (m_MRX_OnStashResult)
			m_MRX_OnStashResult.Invoke(status, assetId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: rate limit, stash point lookup and distance check shared by all stash requests.
	protected MRX_EStashStatus MRX_CheckStashRequest(RplId stashPointId)
	{
		int now = System.GetTickCount();
		if (m_iMRX_LastStashRequestTick != 0 && now - m_iMRX_LastStashRequestTick < MRX_STASH_REQUEST_INTERVAL_MS)
			return MRX_EStashStatus.BUSY;

		m_iMRX_LastStashRequestTick = now;
		if (!MRX_Marx.GetStash())
			return MRX_EStashStatus.STORAGE_ERROR;

		IEntity stashPoint = SCR_EntityHelper.RplIdToEntity(stashPointId);
		if (!stashPoint)
			return MRX_EStashStatus.NOT_IN_INVENTORY;

		MRX_StashPointComponent component = MRX_StashPointComponent.Cast(stashPoint.FindComponent(MRX_StashPointComponent));
		if (!component || !component.IsInRange(GetControlledEntity()))
			return MRX_EStashStatus.REJECTED;

		return MRX_EStashStatus.OK;
	}
}

//------------------------------------------------------------------------------------------------
//! Sends a stash service result back to the requesting player. Internal.
class MRX_StashRpcReply : MRX_StashResultCallback
{
	//! Weak: the controller may be gone when the result arrives.
	protected SCR_PlayerController m_Controller;

	//------------------------------------------------------------------------------------------------
	void MRX_StashRpcReply(SCR_PlayerController controller)
	{
		m_Controller = controller;
	}

	//------------------------------------------------------------------------------------------------
	override void OnResult(MRX_StashResult result)
	{
		super.OnResult(result);
		if (!m_Controller)
			return;

		string assetId;
		MRX_AssetRecord asset = result.GetAsset();
		if (asset)
			assetId = asset.m_sId;

		m_Controller.MRX_SendStashResult(result.m_eStatus, assetId);
	}
}

//------------------------------------------------------------------------------------------------
//! Sends the stash content back to the requesting player. Internal.
class MRX_StashListReply : MRX_StashCallback
{
	//! Weak: the controller may be gone when the result arrives.
	protected SCR_PlayerController m_Controller;

	//------------------------------------------------------------------------------------------------
	void MRX_StashListReply(SCR_PlayerController controller)
	{
		m_Controller = controller;
	}

	//------------------------------------------------------------------------------------------------
	override void OnResult(MRX_EStashStatus status, MRX_StashRecord record)
	{
		super.OnResult(status, record);
		if (m_Controller)
			m_Controller.MRX_SendStashList(status, record);
	}
}
