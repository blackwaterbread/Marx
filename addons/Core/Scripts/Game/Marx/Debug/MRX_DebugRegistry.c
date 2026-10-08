//! The debug actions of the loaded addons, by ID. One per machine (Get), created on first use. Addons add theirs in a
//! modded RegisterActions, after super.RegisterActions().
class MRX_DebugRegistry : Managed
{
	protected static ref MRX_DebugRegistry s_Instance;

	protected ref map<string, ref MRX_DebugAction> m_mActions = new map<string, ref MRX_DebugAction>();
	//! In registration order, for the panel.
	protected ref array<string> m_aIds = {};

	//------------------------------------------------------------------------------------------------
	static MRX_DebugRegistry Get()
	{
		if (!s_Instance)
		{
			s_Instance = new MRX_DebugRegistry();
			s_Instance.RegisterActions();
		}

		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	//! \return False when the ID is empty or taken; the first action with an ID stays.
	bool Register(notnull MRX_DebugAction action)
	{
		string id = action.GetId();
		if (id.IsEmpty() || m_mActions.Contains(id))
		{
			Print(MRX_DebugRunner.LOG_TAG + string.Format("debug action '%1' (%2) not registered: empty or taken ID", id, action.ClassName()), LogLevel.ERROR);
			return false;
		}

		m_mActions.Insert(id, action);
		m_aIds.Insert(id);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	MRX_DebugAction Find(string id)
	{
		return m_mActions.Get(id);
	}

	//------------------------------------------------------------------------------------------------
	//! IDs in registration order.
	int GetIds(notnull array<string> outIds)
	{
		outIds.Copy(m_aIds);
		return outIds.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Pages in the order of their first action.
	int GetPages(notnull array<string> outPages)
	{
		outPages.Clear();
		foreach (string id : m_aIds)
		{
			string page = m_mActions.Get(id).GetPage();
			if (!outPages.Contains(page))
				outPages.Insert(page);
		}

		return outPages.Count();
	}

	//------------------------------------------------------------------------------------------------
	int GetActions(string page, notnull array<MRX_DebugAction> outActions)
	{
		outActions.Clear();
		foreach (string id : m_aIds)
		{
			MRX_DebugAction action = m_mActions.Get(id);
			if (action.GetPage() == page)
				outActions.Insert(action);
		}

		return outActions.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a note on a player's owner ID for marx.info and the panel, e.g. "name-derived". Empty: nothing special.
	string DescribeOwner(int playerId, string ownerId)
	{
		if (ownerId.StartsWith(MRX_IdentityService.NAME_DERIVED_ID_PREFIX))
			return "name-derived";

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Override in a modded class to add actions, after super.RegisterActions().
	protected void RegisterActions()
	{
		Register(new MRX_DebugMarxInfo());
	}
}
