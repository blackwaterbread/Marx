//! What a Marx arsenal panel lists: one category (or all) and search words (client). The category last chosen at a shop
//! is kept for the session. Internal.
class MRX_ArsenalFilter : Managed
{
	//! Last chosen category by shop ID.
	protected static ref map<string, string> s_mCategories;
	//! Lower-case display name and item ID by prefab key and item ID: display names come from item previews.
	protected static ref map<string, string> s_mSearchTexts;

	protected string m_sShopId;
	protected string m_sCategory;
	protected string m_sText;
	protected ref array<string> m_aWords = {};

	//------------------------------------------------------------------------------------------------
	//! \param categories Categories the arsenal lists; a remembered category it no longer lists falls back to all.
	void MRX_ArsenalFilter(string shopId, array<string> categories)
	{
		m_sShopId = shopId;
		if (s_mCategories && s_mCategories.Find(shopId, m_sCategory) && (!categories || !categories.Contains(m_sCategory)))
			m_sCategory = string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Empty for all categories.
	string GetCategory()
	{
		return m_sCategory;
	}

	//------------------------------------------------------------------------------------------------
	string GetText()
	{
		return m_sText;
	}

	//------------------------------------------------------------------------------------------------
	//! \param category Empty for all categories.
	void SetCategory(string category)
	{
		m_sCategory = category;
		if (!s_mCategories)
			s_mCategories = new map<string, string>();

		s_mCategories.Set(m_sShopId, category);
	}

	//------------------------------------------------------------------------------------------------
	//! Words separated by spaces; an item matches when its name or ID contains every word, in any case.
	void SetText(string text)
	{
		m_sText = text;
		text.ToLower();
		m_aWords.Clear();
		text.Split(" ", m_aWords, true);
	}

	//------------------------------------------------------------------------------------------------
	bool Matches(notnull MRX_ShopItem item)
	{
		if (!m_sCategory.IsEmpty() && item.m_sCategory != m_sCategory)
			return false;

		if (m_aWords.IsEmpty())
			return true;

		string searchText = GetSearchText(item);
		foreach (string word : m_aWords)
		{
			if (!searchText.Contains(word))
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static string GetSearchText(notnull MRX_ShopItem item)
	{
		if (!s_mSearchTexts)
			s_mSearchTexts = new map<string, string>();

		string key = MRX_ShopCatalog.GetPrefabKey(item.m_sPrefab) + " " + item.m_sId;
		string text;
		if (s_mSearchTexts.Find(key, text))
			return text;

		if (!item.m_sName.IsEmpty())
			text = WidgetManager.Translate(item.m_sName);
		else
			text = MRX_ScriptedDialog.GetItemDisplayName(item.m_sPrefab);

		text = text + " " + item.m_sId;
		text.ToLower();
		s_mSearchTexts.Set(key, text);
		return text;
	}
}
