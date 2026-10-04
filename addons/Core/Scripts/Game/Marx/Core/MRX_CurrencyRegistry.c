//! Known currencies, keyed by ID.
class MRX_CurrencyRegistry : Managed
{
	protected ref map<string, ref MRX_CurrencyDef> m_mDefs = new map<string, ref MRX_CurrencyDef>();

	//------------------------------------------------------------------------------------------------
	//! \return False when the definition is invalid or its ID is already registered.
	bool Register(MRX_CurrencyDef def)
	{
		if (!def || def.m_sId.IsEmpty())
		{
			Print("[MRX] Currency without ID ignored", LogLevel.ERROR);
			return false;
		}

		if (def.m_iMaxBalance < 0 || def.m_iInitialBalance > def.m_iMaxBalance || def.m_iInitialBalance < def.GetMinBalance())
		{
			Print(string.Format("[MRX] Currency '%1' ignored: initial balance %2 outside limits (max %3)", def.m_sId, def.m_iInitialBalance, def.m_iMaxBalance), LogLevel.ERROR);
			return false;
		}

		if (m_mDefs.Contains(def.m_sId))
		{
			Print(string.Format("[MRX] Currency '%1' registered twice, second definition ignored", def.m_sId), LogLevel.ERROR);
			return false;
		}

		m_mDefs.Insert(def.m_sId, def);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Null for unknown IDs.
	MRX_CurrencyDef Find(string id)
	{
		return m_mDefs.Get(id);
	}

	//------------------------------------------------------------------------------------------------
	int GetIds(notnull array<string> outIds)
	{
		outIds.Clear();
		foreach (string id, MRX_CurrencyDef def : m_mDefs)
		{
			outIds.Insert(id);
		}

		return outIds.Count();
	}
}
