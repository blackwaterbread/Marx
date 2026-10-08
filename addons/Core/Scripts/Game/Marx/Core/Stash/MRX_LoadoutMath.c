//! What putting on a saved loadout costs (API v0): items the player already has are used again, missing ones are bought,
//! the player's other items are sold. With the stash, missing items come from the stash before they are bought, and the
//! player's other items go into the stash instead of being sold. No engine dependencies.
class MRX_LoadoutPlan : Managed
{
	string m_sCurrency;
	//! The loadout without the items that can neither be used again nor bought.
	ref MRX_ItemSnapshot m_Loadout;
	//! Price of the bought items.
	int m_iBuyTotal;
	//! Paid for the player's items the loadout does not use.
	int m_iSellTotal;
	int m_iReusedCount;
	int m_iBoughtCount;
	int m_iSoldCount;
	//! Items of the loadout taken from the stash.
	int m_iStashCount;
	//! The player's items the loadout does not use that go into the stash.
	int m_iStoredCount;
	//! Items of the loadout left out: not the player's, not in the stash and not for sale.
	int m_iUnavailableCount;
	//! Bought items by prefab key (MRX_LoadoutMath.GetPrefabKey).
	ref map<string, int> m_mBought = new map<string, int>();
	//! Buy price of each bought prefab key.
	ref map<string, int> m_mBuyPrices = new map<string, int>();
	//! Items taken from the stash by prefab key.
	ref map<string, int> m_mFromStash = new map<string, int>();
	//! The player's items the loadout does not use (sold, or stored with the stash) by prefab key.
	ref map<string, int> m_mLeftover = new map<string, int>();

	//------------------------------------------------------------------------------------------------
	//! \return Buy total minus sell total: positive to pay, negative to receive.
	int GetNet()
	{
		return m_iBuyTotal - m_iSellTotal;
	}

	//------------------------------------------------------------------------------------------------
	//! True when both plans charge the same for the same items.
	bool IsSameAs(MRX_LoadoutPlan other)
	{
		return other && other.m_iBuyTotal == m_iBuyTotal && other.m_iSellTotal == m_iSellTotal && other.m_iReusedCount == m_iReusedCount
			&& other.m_iBoughtCount == m_iBoughtCount && other.m_iSoldCount == m_iSoldCount && other.m_iStashCount == m_iStashCount
			&& other.m_iStoredCount == m_iStoredCount && other.m_sCurrency == m_sCurrency;
	}

	//------------------------------------------------------------------------------------------------
	//! Price of bought items the player did not get. Missing items count against the stash first (GetStashTake).
	//! \param received Prefab keys of the loadout items the player has after putting it on, with their counts.
	int GetShortfallPrice(notnull map<string, int> received, notnull map<string, int> needed)
	{
		int total;
		foreach (string key, int bought : m_mBought)
		{
			int missing = Math.Max(0, needed.Get(key) - received.Get(key) - m_mFromStash.Get(key));
			missing = Math.Min(bought, missing);
			total = MRX_LoadoutMath.AddCapped(total, MRX_LoadoutMath.MultiplyCapped(m_mBuyPrices.Get(key), missing));
		}

		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! Items of the prefab key to take from the stash once the loadout is on: those planned, less the missing ones.
	int GetStashTake(string key, notnull map<string, int> received, notnull map<string, int> needed)
	{
		int missing = Math.Max(0, needed.Get(key) - received.Get(key));
		return Math.Max(0, m_mFromStash.Get(key) - missing);
	}
}

//------------------------------------------------------------------------------------------------
class MRX_LoadoutMath
{
	//------------------------------------------------------------------------------------------------
	//! \param loadout Saved loadout; its root (the character) is not counted.
	//! \param ownItems Prefabs of the player's items that count (one entry per item).
	//! \param stashItems Prefabs of the stashed items that count, to use the stash; null to not use it.
	static MRX_LoadoutPlan Plan(notnull MRX_ItemSnapshot loadout, notnull array<ResourceName> ownItems, notnull MRX_PriceList prices, array<ResourceName> stashItems = null)
	{
		MRX_LoadoutPlan plan = new MRX_LoadoutPlan();
		plan.m_sCurrency = prices.GetCurrency();

		map<string, ResourceName> ownedPrefabs = new map<string, ResourceName>();
		map<string, int> remaining = CountPrefabs(ownItems, ownedPrefabs);
		map<string, int> stashed;
		if (stashItems)
			stashed = CountPrefabs(stashItems, new map<string, ResourceName>());

		plan.m_Loadout = MRX_ItemSnapshot.Create(loadout.m_sPrefab);
		plan.m_Loadout.m_iFormat = loadout.m_iFormat;
		foreach (MRX_ItemSnapshot child : loadout.m_aChildren)
		{
			MRX_ItemSnapshot kept = PlanItem(child, remaining, stashed, prices, plan);
			if (kept)
				plan.m_Loadout.m_aChildren.Insert(kept);
		}

		foreach (string ownedKey, int left : remaining)
		{
			if (left <= 0)
				continue;

			plan.m_mLeftover.Set(ownedKey, left);
			if (stashItems)
			{
				plan.m_iStoredCount += left;
				continue;
			}

			plan.m_iSoldCount += left;
			int sellPrice = prices.GetSellPrice(ownedPrefabs.Get(ownedKey));
			plan.m_iSellTotal = AddCapped(plan.m_iSellTotal, MultiplyCapped(sellPrice, left));
		}

		return plan;
	}

	//------------------------------------------------------------------------------------------------
	//! Counts every item below the root by prefab key.
	static void CountItems(notnull MRX_ItemSnapshot root, notnull map<string, int> outCounts)
	{
		foreach (MRX_ItemSnapshot child : root.m_aChildren)
		{
			string key = GetPrefabKey(child.m_sPrefab);
			outCounts.Set(key, outCounts.Get(key) + 1);
			CountItems(child, outCounts);
		}
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

	//------------------------------------------------------------------------------------------------
	//! a + b, at most int.MAX (both not negative).
	static int AddCapped(int a, int b)
	{
		if (a > int.MAX - b)
			return int.MAX;

		return a + b;
	}

	//------------------------------------------------------------------------------------------------
	//! price * count, at most int.MAX (both not negative).
	static int MultiplyCapped(int price, int count)
	{
		if (price <= 0 || count <= 0)
			return 0;

		if (price > int.MAX / count)
			return int.MAX;

		return price * count;
	}

	//------------------------------------------------------------------------------------------------
	//! \param outPrefabs A prefab of each counted key.
	//! \return Number of items by prefab key.
	protected static map<string, int> CountPrefabs(notnull array<ResourceName> prefabs, notnull map<string, ResourceName> outPrefabs)
	{
		map<string, int> counts = new map<string, int>();
		foreach (ResourceName prefab : prefabs)
		{
			string key = GetPrefabKey(prefab);
			if (key.IsEmpty())
				continue;

			counts.Set(key, counts.Get(key) + 1);
			outPrefabs.Set(key, prefab);
		}

		return counts;
	}

	//------------------------------------------------------------------------------------------------
	//! Decides one item and then its contents (an item left out takes its contents with it).
	//! \param stashed Stashed items left by prefab key, or null when the stash is not used.
	//! \return Copy of the item with the kept contents, or null when it is left out.
	protected static MRX_ItemSnapshot PlanItem(notnull MRX_ItemSnapshot item, notnull map<string, int> remaining, map<string, int> stashed, notnull MRX_PriceList prices, notnull MRX_LoadoutPlan plan)
	{
		string key = GetPrefabKey(item.m_sPrefab);
		int left = remaining.Get(key);
		if (left > 0)
		{
			remaining.Set(key, left - 1);
			plan.m_iReusedCount++;
		}
		else if (stashed && stashed.Get(key) > 0)
		{
			stashed.Set(key, stashed.Get(key) - 1);
			plan.m_iStashCount++;
			plan.m_mFromStash.Set(key, plan.m_mFromStash.Get(key) + 1);
		}
		else
		{
			int price = prices.GetBuyPrice(item.m_sPrefab);
			if (price <= 0)
			{
				plan.m_iUnavailableCount += item.CountItems();
				return null;
			}

			plan.m_iBoughtCount++;
			plan.m_iBuyTotal = AddCapped(plan.m_iBuyTotal, price);
			plan.m_mBought.Set(key, plan.m_mBought.Get(key) + 1);
			plan.m_mBuyPrices.Set(key, price);
		}

		MRX_ItemSnapshot kept = item.Copy();
		kept.m_aChildren.Clear();
		foreach (MRX_ItemSnapshot child : item.m_aChildren)
		{
			MRX_ItemSnapshot keptChild = PlanItem(child, remaining, stashed, prices, plan);
			if (keptChild)
				kept.m_aChildren.Insert(keptChild);
		}

		return kept;
	}
}
