#ifdef WORKBENCH
//! Workbench only: "#marxkit" in the chat puts a few different backpacks and small items into the executing player's
//! stash, for trying the stash panel by hand (moving items between bags). Close and reopen an open stash to see them.
class MRX_TestKitCommand : ScrServerCommand
{
	static const string KEYWORD = "marxkit";
	protected static const int BACKPACK_COUNT = 3;
	protected static const ResourceName FALLBACK_BACKPACK = "{06B68C58B72EAAC6}Prefabs/Items/Equipment/Backpacks/Backpack_ALICE_Medium.et";

	//------------------------------------------------------------------------------------------------
	override string GetKeyword()
	{
		return KEYWORD;
	}

	//------------------------------------------------------------------------------------------------
	override bool IsServerSide()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override int RequiredRCONPermission()
	{
		return ERCONPermissions.PERMISSIONS_ADMIN;
	}

	//------------------------------------------------------------------------------------------------
	override int RequiredChatPermission()
	{
		return EPlayerRole.ADMINISTRATOR;
	}

	//------------------------------------------------------------------------------------------------
	override ref ScrServerCmdResult OnChatClientExecution(array<string> argv, int playerId)
	{
		return ScrServerCmdResult(string.Empty, EServerCmdResultType.OK);
	}

	//------------------------------------------------------------------------------------------------
	override ref ScrServerCmdResult OnChatServerExecution(array<string> argv, int playerId)
	{
		MRX_StashService stash = MRX_Marx.GetStash();
		string ownerId = MRX_Marx.GetOwnerId(playerId);
		if (!stash || ownerId.IsEmpty())
			return ScrServerCmdResult("No stash or no owner for you (start Workbench with -mrxTestIdentity)", EServerCmdResultType.ERR);

		array<ResourceName> prefabs = {};
		FindBackpacks(prefabs);
		GetSmallItems(prefabs);
		foreach (ResourceName prefab : prefabs)
		{
			stash.Grant(ownerId, prefab, MRX_TxContext.Create("marx_testkit", "test kit", "testkit:" + MRX_Marx.NewId()));
		}

		return ScrServerCmdResult(string.Format("%1 items go into your stash; reopen it to see them", prefabs.Count()), EServerCmdResultType.OK);
	}

	//------------------------------------------------------------------------------------------------
	override ref ScrServerCmdResult OnRCONExecution(array<string> argv)
	{
		return ScrServerCmdResult("Use it in the chat", EServerCmdResultType.ERR);
	}

	//------------------------------------------------------------------------------------------------
	override ref ScrServerCmdResult OnUpdate()
	{
		return ScrServerCmdResult(string.Empty, EServerCmdResultType.OK);
	}

	//------------------------------------------------------------------------------------------------
	protected static void GetSmallItems(notnull array<ResourceName> outPrefabs)
	{
		outPrefabs.Insert("{A81F501D3EF6F38E}Prefabs/Items/Medicine/FieldDressing_01/FieldDressing_US_01.et");
		outPrefabs.Insert("{A81F501D3EF6F38E}Prefabs/Items/Medicine/FieldDressing_01/FieldDressing_US_01.et");
		outPrefabs.Insert("{D70216B1B2889129}Prefabs/Items/Medicine/Tourniquet_01/Tourniquet_US_01.et");
		outPrefabs.Insert("{0D9A5DCF89AE7AA9}Prefabs/Items/Medicine/MorphineInjection_01/MorphineInjection_01.et");
		outPrefabs.Insert("{61D4F80E49BF9B12}Prefabs/Items/Equipment/Compass/Compass_SY183.et");
		outPrefabs.Insert("{6FD6C96121905202}Prefabs/Items/Equipment/Watches/Watch_Vostok.et");
		outPrefabs.Insert("{0CF54B9A85D8E0D4}Prefabs/Items/Equipment/Binoculars/Binoculars_M22/Binoculars_M22.et");
	}

	//------------------------------------------------------------------------------------------------
	//! Different backpacks from the US item catalog, or the ALICE backpack when there is none.
	protected static void FindBackpacks(notnull array<ResourceName> outPrefabs)
	{
		SCR_Faction faction = SCR_Faction.Cast(GetGame().GetFactionManager().GetFactionByKey("US"));
		SCR_EntityCatalog catalog;
		if (faction)
			catalog = faction.GetFactionEntityCatalogOfType(EEntityCatalogType.ITEM);

		if (catalog)
		{
			array<SCR_EntityCatalogEntry> entries = {};
			catalog.GetEntityList(entries);
			foreach (SCR_EntityCatalogEntry entry : entries)
			{
				ResourceName prefab = entry.GetPrefab();
				if (prefab.Contains("Backpack") && !outPrefabs.Contains(prefab))
					outPrefabs.Insert(prefab);

				if (outPrefabs.Count() >= BACKPACK_COUNT)
					return;
			}
		}

		while (outPrefabs.Count() < BACKPACK_COUNT)
		{
			outPrefabs.Insert(FALLBACK_BACKPACK);
		}
	}
}
#endif
