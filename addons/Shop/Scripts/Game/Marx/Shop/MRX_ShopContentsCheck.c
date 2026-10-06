//! Default contents of one catalog item, as measured by MRX_ShopContentsCheck (API v0).
class MRX_ShopContentsEntry : Managed
{
	string m_sItemId;
	ResourceName m_sPrefab;
	string m_sCurrency;
	int m_iPrice;
	//! Price the shop pays for the item alone.
	int m_iSellPrice;
	//! Items the prefab holds when it is spawned (attachments, magazines, stored items), recursively.
	int m_iContentsCount;
	//! Of those, items not in the catalog or in another currency (worth nothing to the shop).
	int m_iUnknownCount;
	//! Sum of the catalog purchase prices of the contents.
	int m_iContentsPrice;
	//! Sum of the shop's sell prices of the contents.
	int m_iContentsSell;
	//! False when the prefab could not be spawned.
	bool m_bMeasured;

	//------------------------------------------------------------------------------------------------
	//! True when selling the item as bought (or its parts one by one) pays more than its price.
	bool IsProfitable()
	{
		return m_bMeasured && m_iSellPrice + m_iContentsSell > m_iPrice;
	}
}

void MRX_ShopContentsCheckDelegate(array<ref MRX_ShopContentsEntry> entries);
typedef func MRX_ShopContentsCheckDelegate;

//------------------------------------------------------------------------------------------------
//! Catalog check (API v0, server, development tool): spawns every item a shop sells once, locally, records what the
//! prefab holds by default and deletes it again. A catalog price must cover the default contents, or players earn money
//! by buying an item and selling it back (in one piece through a Marx arsenal, or its parts one by one).
//! Takes about 15 ms per item, spread over frames.
class MRX_ShopContentsCheck : Managed
{
	protected static const int BATCH_SIZE = 20;
	protected static const int SETTLE_MS = 300;
	protected static const float SPACING = 3;

	protected ref MRX_ShopDefinition m_Shop;
	protected vector m_vPosition;
	protected ref array<ref MRX_ShopContentsEntry> m_aEntries = {};
	protected ref array<MRX_ShopItem> m_aPending = {};
	protected ref array<IEntity> m_aSpawned = {};
	protected ref array<ref MRX_ShopContentsEntry> m_aBatch = {};
	protected ref ScriptInvokerBase<MRX_ShopContentsCheckDelegate> m_OnDone;
	protected bool m_bRunning;

	//------------------------------------------------------------------------------------------------
	//! Invoked once with one entry per listed item (price above 0), in catalog order.
	ScriptInvokerBase<MRX_ShopContentsCheckDelegate> GetOnDone()
	{
		if (!m_OnDone)
			m_OnDone = new ScriptInvokerBase<MRX_ShopContentsCheckDelegate>();

		return m_OnDone;
	}

	//------------------------------------------------------------------------------------------------
	//! \param position Where the items are spawned, side by side; somewhere nobody is (e.g. high above the map).
	//! \return False when a check is already running.
	bool Start(notnull MRX_ShopDefinition shop, vector position)
	{
		if (m_bRunning)
			return false;

		m_bRunning = true;
		m_Shop = shop;
		m_vPosition = position;
		m_aEntries.Clear();
		m_aPending.Clear();
		foreach (MRX_ShopItem item : shop.m_Catalog.m_aItems)
		{
			if (item.m_iPrice > 0)
				m_aPending.Insert(item);
		}

		GetGame().GetCallqueue().CallLater(SpawnBatch);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes the entries as CSV (one header line), e.g. for a price generator.
	//! \return False when the file cannot be written.
	static bool WriteReport(string path, notnull array<ref MRX_ShopContentsEntry> entries)
	{
		FileHandle file = FileIO.OpenFile(path, FileMode.WRITE);
		if (!file)
			return false;

		file.WriteLine("item_id,prefab,currency,price,sell_price,contents,unknown_contents,contents_price,contents_sell,profitable,measured");
		foreach (MRX_ShopContentsEntry entry : entries)
		{
			int profitable, measured;
			if (entry.IsProfitable())
				profitable = 1;

			if (entry.m_bMeasured)
				measured = 1;

			string line = string.Format("%1,%2,%3,%4,%5,%6,%7,%8,%9", entry.m_sItemId, entry.m_sPrefab, entry.m_sCurrency, entry.m_iPrice,
				entry.m_iSellPrice, entry.m_iContentsCount, entry.m_iUnknownCount, entry.m_iContentsPrice, entry.m_iContentsSell);
			file.WriteLine(string.Format("%1,%2,%3", line, profitable, measured));
		}

		file.Close();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void SpawnBatch()
	{
		m_aSpawned.Clear();
		m_aBatch.Clear();
		while (m_aBatch.Count() < BATCH_SIZE && !m_aPending.IsEmpty())
		{
			MRX_ShopItem item = m_aPending[0];
			m_aPending.RemoveOrdered(0);

			MRX_ShopContentsEntry entry = new MRX_ShopContentsEntry();
			entry.m_sItemId = item.m_sId;
			entry.m_sPrefab = item.m_sPrefab;
			entry.m_sCurrency = item.m_sCurrency;
			entry.m_iPrice = item.m_iPrice;
			entry.m_iSellPrice = m_Shop.GetSellPrice(item);
			m_aEntries.Insert(entry);

			IEntity spawned;
			Resource resource = Resource.Load(item.m_sPrefab);
			if (resource && resource.IsValid())
			{
				EntitySpawnParams params = new EntitySpawnParams();
				params.TransformMode = ETransformMode.WORLD;
				params.Transform[3] = m_vPosition + Vector(m_aBatch.Count() * SPACING, 0, 0);
				spawned = GetGame().SpawnEntityPrefabLocal(resource, GetGame().GetWorld(), params);
			}

			m_aBatch.Insert(entry);
			m_aSpawned.Insert(spawned);
		}

		// Attachments and stored items are complete a moment after the spawn.
		GetGame().GetCallqueue().CallLater(MeasureBatch, SETTLE_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void MeasureBatch()
	{
		foreach (int i, MRX_ShopContentsEntry entry : m_aBatch)
		{
			IEntity spawned = m_aSpawned[i];
			if (!spawned)
				continue;

			Measure(entry, MRX_EntitySnapshots.Capture(spawned));
			entry.m_bMeasured = true;
			SCR_EntityHelper.DeleteEntityAndChildren(spawned);
		}

		if (!m_aPending.IsEmpty())
		{
			SpawnBatch();
			return;
		}

		m_bRunning = false;
		m_aSpawned.Clear();
		m_aBatch.Clear();
		if (m_OnDone)
			m_OnDone.Invoke(m_aEntries);
	}

	//------------------------------------------------------------------------------------------------
	protected void Measure(notnull MRX_ShopContentsEntry entry, notnull MRX_ItemSnapshot snapshot)
	{
		foreach (MRX_ItemSnapshot child : snapshot.m_aChildren)
		{
			entry.m_iContentsCount++;
			MRX_ShopItem item = m_Shop.m_Catalog.FindByPrefab(child.m_sPrefab);
			if (item && item.m_sCurrency == entry.m_sCurrency)
			{
				entry.m_iContentsPrice += item.m_iPrice;
				entry.m_iContentsSell += m_Shop.GetSellPrice(item);
			}
			else
			{
				entry.m_iUnknownCount++;
			}

			Measure(entry, child);
		}
	}
}
