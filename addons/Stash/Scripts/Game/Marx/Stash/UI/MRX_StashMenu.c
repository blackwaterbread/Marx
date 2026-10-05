//! Minimal stash dialog (API v0): stashed assets with Withdraw, carried items with Deposit.
//! Client side; the content comes from the server and every request is validated again there.
class MRX_StashMenu : MRX_ScriptedDialog
{
	//! Weak: the stash point may stream out while the dialog is open.
	protected IEntity m_StashPoint;
	protected TextWidget m_wStatus;

	protected ref array<string> m_aAssetIds = {};
	protected ref array<string> m_aPrefabs = {};
	protected ref array<int> m_aStates = {};
	//! Carried items, by index in the "deposit:<index>" actions.
	protected ref array<IEntity> m_aDepositItems = {};

	//------------------------------------------------------------------------------------------------
	static MRX_StashMenu Open(notnull MRX_StashPointComponent stashPoint)
	{
		MRX_StashMenu menu = new MRX_StashMenu();
		menu.m_StashPoint = stashPoint.GetOwner();
		OpenDialog(menu, "Stash", "MRX_Stash");
		return menu;
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnDialogOpened()
	{
		m_wStatus = AddHeaderLine("Loading...");

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller || !m_StashPoint)
			return;

		controller.MRX_GetOnStashList().Insert(OnStashList);
		controller.MRX_GetOnStashResult().Insert(OnStashResult);
		controller.MRX_RequestStashList(m_StashPoint);
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuClose()
	{
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (controller)
		{
			controller.MRX_GetOnStashList().Remove(OnStashList);
			controller.MRX_GetOnStashResult().Remove(OnStashResult);
		}

		super.OnMenuClose();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnStashList(MRX_EStashStatus status, array<string> assetIds, array<string> prefabs, array<int> states)
	{
		if (status != MRX_EStashStatus.OK)
		{
			m_wStatus.SetText("Stash unavailable: " + typename.EnumToString(MRX_EStashStatus, status));
			return;
		}

		m_aAssetIds.Copy(assetIds);
		m_aPrefabs.Copy(prefabs);
		m_aStates.Copy(states);
		m_wStatus.SetText(string.Format("%1 stored", m_aAssetIds.Count()));
		BuildRows();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnStashResult(MRX_EStashStatus status, string assetId)
	{
		if (status == MRX_EStashStatus.OK)
			m_wStatus.SetText("Done");
		else
			m_wStatus.SetText("Failed: " + typename.EnumToString(MRX_EStashStatus, status));

		// The list request must respect the server's request interval.
		GetGame().GetCallqueue().CallLater(RequestList, 300);
	}

	//------------------------------------------------------------------------------------------------
	protected void RequestList()
	{
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (controller && m_StashPoint)
			controller.MRX_RequestStashList(m_StashPoint);
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildRows()
	{
		ClearRows();
		m_aDepositItems.Clear();

		foreach (int i, string assetId : m_aAssetIds)
		{
			string name = GetPrefabDisplayName(m_aPrefabs[i]);
			if (m_aStates[i] == MRX_EAssetState.STASHED)
				AddRow(name, "Withdraw", "withdraw:" + assetId);
			else
				AddRow(name + "  (in use)");
		}

		array<IEntity> carried = {};
		InventoryStorageManagerComponent manager = GetLocalStorageManager();
		if (manager)
			manager.GetItems(carried);

		foreach (IEntity item : carried)
		{
			// Magazines and attachments go with their weapon.
			IEntity parent = item.GetParent();
			if (parent && parent.FindComponent(BaseWeaponComponent))
				continue;

			int index = m_aDepositItems.Insert(item);
			AddRow(GetPrefabDisplayName(SCR_ResourceNameUtils.GetPrefabName(item)), "Deposit", "deposit:" + index.ToString());
		}
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnRowAction(string action)
	{
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller || !m_StashPoint)
			return;

		if (action.StartsWith("withdraw:"))
		{
			controller.MRX_RequestStashWithdraw(m_StashPoint, action.Substring(9, action.Length() - 9));
			m_wStatus.SetText("...");
			return;
		}

		int index = action.Substring(8, action.Length() - 8).ToInt();
		if (index >= 0 && index < m_aDepositItems.Count() && m_aDepositItems[index])
		{
			controller.MRX_RequestStashDeposit(m_StashPoint, m_aDepositItems[index]);
			m_wStatus.SetText("...");
		}
	}
}
