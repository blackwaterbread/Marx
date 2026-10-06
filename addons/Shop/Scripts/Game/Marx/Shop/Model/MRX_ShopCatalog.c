//! List of items a shop sells and buys back (API v0).
[BaseContainerProps(configRoot: true)]
class MRX_ShopCatalog
{
	[Attribute()]
	ref array<ref MRX_ShopItem> m_aItems;

	//------------------------------------------------------------------------------------------------
	//! \return Null for unknown IDs.
	MRX_ShopItem FindItem(string id)
	{
		if (!m_aItems)
			return null;

		foreach (MRX_ShopItem item : m_aItems)
		{
			if (item.m_sId == id)
				return item;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! \return First item with the same prefab, or null. Products (MRX_ShopItem.m_Product) are not matched.
	MRX_ShopItem FindByPrefab(ResourceName prefab)
	{
		if (!m_aItems || prefab.IsEmpty())
			return null;

		string key = GetPrefabKey(prefab);
		foreach (MRX_ShopItem item : m_aItems)
		{
			if (!item.m_Product && GetPrefabKey(item.m_sPrefab) == key)
				return item;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Removes invalid entries (empty or duplicate ID, no prefab, unknown currency) and logs them.
	//! \return Number of entries kept.
	//! \param currencies Null skips the currency check (clients have no registry).
	int Validate(MRX_CurrencyRegistry currencies, string catalogName)
	{
		if (!m_aItems)
			m_aItems = {};

		array<string> seenIds = {};
		for (int i = m_aItems.Count() - 1; i >= 0; i--)
		{
			MRX_ShopItem item = m_aItems[i];
			string problem;
			if (!item || item.m_sId.IsEmpty())
				problem = "missing ID";
			else if (item.m_sPrefab.IsEmpty() && !item.m_Product)
				problem = "missing prefab";
			else if (currencies && !currencies.Find(item.m_sCurrency))
				problem = "unknown currency '" + item.m_sCurrency + "'";

			if (problem.IsEmpty())
				continue;

			string itemId;
			if (item)
				itemId = item.m_sId;

			Print(string.Format("[MRX] Shop catalog %1: item '%2' ignored, %3", catalogName, itemId, problem), LogLevel.ERROR);
			m_aItems.RemoveOrdered(i);
		}

		// Duplicates: the first entry wins.
		for (int j = 0; j < m_aItems.Count(); )
		{
			string id = m_aItems[j].m_sId;
			if (seenIds.Contains(id))
			{
				Print(string.Format("[MRX] Shop catalog %1: duplicate item ID '%2' ignored", catalogName, id), LogLevel.ERROR);
				m_aItems.RemoveOrdered(j);
				continue;
			}

			seenIds.Insert(id);
			j++;
		}

		return m_aItems.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Comparable form of a prefab ResourceName: the GUID when present, otherwise the lower-case path.
	static string GetPrefabKey(ResourceName prefab)
	{
		string text = prefab;
		if (text.StartsWith("{"))
		{
			int end = text.IndexOf("}");
			if (end > 0)
				return text.Substring(0, end + 1);
		}

		text.ToLower();
		return text;
	}
}
