//! Debug actions of the "Stash" page: the stash and the saved loadouts.

//------------------------------------------------------------------------------------------------
modded class MRX_DebugRegistry
{
	//------------------------------------------------------------------------------------------------
	override protected void RegisterActions()
	{
		super.RegisterActions();
		Register(new MRX_DebugStashOpen());
		Register(new MRX_DebugStashKit());
		Register(new MRX_DebugStashFromHands());
		Register(new MRX_DebugLoadoutSave());
		Register(new MRX_DebugLoadoutLoad());
		Register(new MRX_DebugLoadoutWindow());
		Register(new MRX_DebugLoadoutAddSlot());
	}
}

//------------------------------------------------------------------------------------------------
class MRX_DebugStashUtils
{
	//------------------------------------------------------------------------------------------------
	static MRX_TxContext CreateTxContext(string actionId)
	{
		return MRX_TxContext.Create(MRX_DebugWallet.LEDGER_SOURCE, actionId, "debug:" + MRX_Marx.NewId());
	}

	//------------------------------------------------------------------------------------------------
	//! \return The player's open stash, or null.
	static MRX_StashSession FindSession(int playerId)
	{
		MRX_StashSessionManager sessions = MRX_StashSessions.Get();
		if (!sessions)
			return null;

		return sessions.Find(playerId);
	}
}

//------------------------------------------------------------------------------------------------
//! Opens the stash at the nearest stash point within SEARCH_RADIUS, spawning the sample stash wardrobe in front of the
//! player when there is none.
class MRX_DebugStashOpen : MRX_DebugAction
{
	static const float SEARCH_RADIUS = 30;
	protected static const ResourceName STASH_PREFAB = "{66BACE8BD545B8C2}Prefabs/Marx/Stash/MRX_StashWardrobe.et";
	protected static const float SPAWN_DISTANCE = 1.2;

	//------------------------------------------------------------------------------------------------
	void MRX_DebugStashOpen()
	{
		Setup("stash.open", "Stash", "Open stash");
	}

	//------------------------------------------------------------------------------------------------
	override bool HasServerPart()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool HasClientPart()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void RunServer(notnull MRX_DebugContext context, notnull MRX_DebugReply reply)
	{
		if (!context.m_Character)
		{
			reply.Done(MRX_DebugActionUtils.NoCharacter());
			return;
		}

		string found = "nearby stash point";
		IEntity stashPoint = MRX_DebugActionUtils.FindNear(context.m_Character.GetOrigin(), SEARCH_RADIUS, MRX_StashPointComponent);
		if (!stashPoint)
		{
			found = "spawned the sample stash wardrobe";
			stashPoint = MRX_DebugActionUtils.SpawnInFront(context.m_Character, STASH_PREFAB, SPAWN_DISTANCE);
		}

		if (!stashPoint || !stashPoint.FindComponent(MRX_StashPointComponent))
		{
			reply.Done(MRX_DebugResult.Failed("No stash point nearby and the sample stash wardrobe did not spawn"));
			return;
		}

		reply.Done(MRX_DebugResult.Ok(string.Format("%1 %2", found, MRX_DebugActionUtils.GetPrefabFileName(stashPoint))).SetEntity(stashPoint));
	}

	//------------------------------------------------------------------------------------------------
	override MRX_DebugResult RunClient(notnull MRX_DebugContext context, notnull MRX_DebugResult serverResult)
	{
		if (!context.m_Controller || !context.m_Entity)
			return MRX_DebugResult.Failed("The stash point is not here");

		// The stash point checks the distance; a failure shows as a hint.
		context.m_Controller.MRX_RequestStashOpen(context.m_Entity);
		return null;
	}
}

//------------------------------------------------------------------------------------------------
//! Puts a few different backpacks and small items into the player's stash, for trying the stash panel (moving items
//! between bags). An open stash shows them after reopening it.
class MRX_DebugStashKit : MRX_DebugAction
{
	protected static const int BACKPACK_COUNT = 3;
	protected static const ResourceName FALLBACK_BACKPACK = "{06B68C58B72EAAC6}Prefabs/Items/Equipment/Backpacks/Backpack_ALICE_Medium.et";

	//------------------------------------------------------------------------------------------------
	void MRX_DebugStashKit()
	{
		Setup("stash.kit", "Stash", "Give kit");
	}

	//------------------------------------------------------------------------------------------------
	override bool HasServerPart()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void RunServer(notnull MRX_DebugContext context, notnull MRX_DebugReply reply)
	{
		MRX_StashService stash = MRX_Marx.GetStash();
		string ownerId = MRX_Marx.GetOwnerId(context.m_iPlayerId);
		if (!stash || ownerId.IsEmpty())
		{
			reply.Done(MRX_DebugResult.Failed("No stash or no owner ID for you (in Workbench start with -mrxTestIdentity)"));
			return;
		}

		array<ResourceName> prefabs = {};
		FindBackpacks(prefabs);
		GetSmallItems(prefabs);
		foreach (ResourceName prefab : prefabs)
		{
			stash.Grant(ownerId, prefab, MRX_DebugStashUtils.CreateTxContext(GetId()));
		}

		reply.Done(MRX_DebugResult.Ok(string.Format("%1 items go into your stash; reopen it to see them", prefabs.Count())));
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
		SCR_Faction faction;
		if (GetGame().GetFactionManager())
			faction = SCR_Faction.Cast(GetGame().GetFactionManager().GetFactionByKey("US"));

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

//------------------------------------------------------------------------------------------------
//! Moves the weapon in the player's hands into the open stash.
class MRX_DebugStashFromHands : MRX_DebugAction
{
	//------------------------------------------------------------------------------------------------
	void MRX_DebugStashFromHands()
	{
		Setup("stash.fromhands", "Stash", "Weapon to stash");
	}

	//------------------------------------------------------------------------------------------------
	override bool HasServerPart()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void RunServer(notnull MRX_DebugContext context, notnull MRX_DebugReply reply)
	{
		ChimeraCharacter character = context.GetChimera();
		if (!character || !character.GetCharacterController())
		{
			reply.Done(MRX_DebugActionUtils.NoCharacter());
			return;
		}

		MRX_StashSession session = MRX_DebugStashUtils.FindSession(context.m_iPlayerId);
		if (!session || !session.GetStorage())
		{
			reply.Done(MRX_DebugResult.Failed("Open a stash first (stash.open)"));
			return;
		}

		BaseWeaponManagerComponent weapons = BaseWeaponManagerComponent.Cast(character.FindComponent(BaseWeaponManagerComponent));
		if (!weapons || !weapons.GetCurrentWeapon())
		{
			reply.Done(MRX_DebugResult.Failed("No weapon in your hands"));
			return;
		}

		IEntity weapon = weapons.GetCurrentWeapon().GetOwner();
		if (!character.GetCharacterController().GetInventoryStorageManager().TryMoveItemToStorage(weapon, session.GetStorage()))
		{
			reply.Done(MRX_DebugResult.Failed("The inventory refused the move"));
			return;
		}

		reply.Done(MRX_DebugResult.Ok(MRX_DebugActionUtils.GetPrefabFileName(weapon)));
	}
}

//------------------------------------------------------------------------------------------------
//! Saves the player's gear into a loadout slot (0-based) of the open stash.
class MRX_DebugLoadoutSave : MRX_DebugAction
{
	//------------------------------------------------------------------------------------------------
	void MRX_DebugLoadoutSave()
	{
		Setup("loadout.save", "Stash", "Save loadout");
		AddArg("slot", "0", true);
	}

	//------------------------------------------------------------------------------------------------
	override bool HasServerPart()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void RunServer(notnull MRX_DebugContext context, notnull MRX_DebugReply reply)
	{
		MRX_LoadoutService loadouts = MRX_Loadouts.Get();
		if (!loadouts)
		{
			reply.Done(MRX_DebugResult.Failed("Loadouts are not running on this server"));
			return;
		}

		if (!MRX_Marx.GetPriceList())
		{
			reply.Done(MRX_DebugResult.Failed("No price list; the consumer sets one, in Marx Example run sample.prices"));
			return;
		}

		loadouts.Save(context.m_iPlayerId, context.GetInt(0), new MRX_DebugLoadoutReply(reply, context.m_Controller));
	}
}

//------------------------------------------------------------------------------------------------
//! Puts on the loadout of a slot (0-based) as the Load button of the open loadout window does.
class MRX_DebugLoadoutLoad : MRX_DebugAction
{
	//------------------------------------------------------------------------------------------------
	void MRX_DebugLoadoutLoad()
	{
		Setup("loadout.load", "Stash", "Load loadout");
		AddArg("slot", "0", true);
	}

	//------------------------------------------------------------------------------------------------
	override bool HasClientPart()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override MRX_DebugResult RunClient(notnull MRX_DebugContext context, notnull MRX_DebugResult serverResult)
	{
		MRX_LoadoutMenu menu = MRX_LoadoutMenu.GetOpen();
		if (!menu || !menu.GetBar())
			return MRX_DebugResult.Failed("Open the loadout window first (loadout.window)");

		menu.GetBar().RequestLoad(context.GetInt(0));
		return MRX_DebugResult.Ok("requested; the loadout window shows the result");
	}
}

//------------------------------------------------------------------------------------------------
//! Opens the loadout window (it needs an open stash to act).
class MRX_DebugLoadoutWindow : MRX_DebugAction
{
	//------------------------------------------------------------------------------------------------
	void MRX_DebugLoadoutWindow()
	{
		Setup("loadout.window", "Stash", "Loadout window");
	}

	//------------------------------------------------------------------------------------------------
	override bool HasClientPart()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override MRX_DebugResult RunClient(notnull MRX_DebugContext context, notnull MRX_DebugResult serverResult)
	{
		MRX_LoadoutMenu.Open();
		return MRX_DebugResult.Ok();
	}
}

//------------------------------------------------------------------------------------------------
//! Unlocks one more loadout slot for the player (shown from the next opening of the stash).
class MRX_DebugLoadoutAddSlot : MRX_DebugAction
{
	//------------------------------------------------------------------------------------------------
	void MRX_DebugLoadoutAddSlot()
	{
		Setup("loadout.addslot", "Stash", "Add loadout slot");
	}

	//------------------------------------------------------------------------------------------------
	override bool HasServerPart()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void RunServer(notnull MRX_DebugContext context, notnull MRX_DebugReply reply)
	{
		string ownerId = MRX_Marx.GetOwnerId(context.m_iPlayerId);
		if (ownerId.IsEmpty())
		{
			reply.Done(MRX_DebugResult.Failed("You have no owner ID yet"));
			return;
		}

		MRX_LoadoutSlots.AddSlots(ownerId, 1, MRX_DebugStashUtils.CreateTxContext(GetId()), new MRX_DebugLoadoutSlotsReply(reply));
	}
}

//------------------------------------------------------------------------------------------------
//! Answers a loadout save and shows the result in the player's loadout window too. Internal.
class MRX_DebugLoadoutReply : MRX_LoadoutCallback
{
	protected ref MRX_DebugReply m_Reply;
	//! Weak: the controller may be gone when the result arrives.
	protected SCR_PlayerController m_Controller;

	//------------------------------------------------------------------------------------------------
	void MRX_DebugLoadoutReply(MRX_DebugReply reply, SCR_PlayerController controller)
	{
		m_Reply = reply;
		m_Controller = controller;
	}

	//------------------------------------------------------------------------------------------------
	override void OnResult(MRX_LoadoutResult result)
	{
		if (m_Controller)
			m_Controller.MRX_DeliverLoadoutResult(result);

		string text = string.Format("slot %1: %2", result.m_iSlot, typename.EnumToString(MRX_ELoadoutStatus, result.m_eStatus));
		if (result.m_eStatus == MRX_ELoadoutStatus.OK)
			m_Reply.Done(MRX_DebugResult.Ok(text));
		else
			m_Reply.Done(MRX_DebugResult.Failed(text));
	}
}

//------------------------------------------------------------------------------------------------
//! Answers MRX_LoadoutSlots.AddSlots. Internal.
class MRX_DebugLoadoutSlotsReply : MRX_LoadoutSlotsCallback
{
	protected ref MRX_DebugReply m_Reply;

	//------------------------------------------------------------------------------------------------
	void MRX_DebugLoadoutSlotsReply(MRX_DebugReply reply)
	{
		m_Reply = reply;
	}

	//------------------------------------------------------------------------------------------------
	override void OnResult(MRX_EStashStatus status, bool limitReached, int slots)
	{
		string text = string.Format("%1 slots", slots);
		if (status != MRX_EStashStatus.OK)
			m_Reply.Done(MRX_DebugResult.Failed(string.Format("%1 (%2)", typename.EnumToString(MRX_EStashStatus, status), text)));
		else if (limitReached)
			m_Reply.Done(MRX_DebugResult.Failed("Limit reached, " + text));
		else
			m_Reply.Done(MRX_DebugResult.Ok(text + "; the stash shows them from its next opening"));
	}
}
