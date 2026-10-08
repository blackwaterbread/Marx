//! Debug actions of the "Marx" page: the player, the character and the Marx services.

//------------------------------------------------------------------------------------------------
//! Shared helpers of the debug actions.
class MRX_DebugActionUtils
{
	//------------------------------------------------------------------------------------------------
	//! "Rifle_M16A2.et" for an entity of that prefab.
	static string GetPrefabFileName(IEntity entity)
	{
		if (!entity)
			return string.Empty;

		return FilePath.StripPath(SCR_ResourceNameUtils.GetPrefabName(entity));
	}

	//------------------------------------------------------------------------------------------------
	//! \return The character's controller, or null when the context has no character.
	static CharacterControllerComponent GetCharacterController(notnull MRX_DebugContext context)
	{
		ChimeraCharacter character = context.GetChimera();
		if (!character)
			return null;

		return character.GetCharacterController();
	}

	//------------------------------------------------------------------------------------------------
	static MRX_DebugResult NoCharacter()
	{
		return MRX_DebugResult.Failed("You have no character");
	}

	//------------------------------------------------------------------------------------------------
	//! The nearest entity with the component within the radius, or null.
	static IEntity FindNear(vector center, float radius, typename componentType)
	{
		MRX_DebugEntityQuery query = new MRX_DebugEntityQuery(center, componentType);
		GetGame().GetWorld().QueryEntitiesBySphere(center, radius, query.OnEntity);
		return query.GetFound();
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns a prefab on the ground in front of the character.
	static IEntity SpawnInFront(notnull IEntity character, ResourceName prefab, float distance)
	{
		vector position = character.GetOrigin() + character.GetTransformAxis(2) * distance;
		position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]);
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = position;
		return GetGame().SpawnEntityPrefab(Resource.Load(prefab), GetGame().GetWorld(), params);
	}
}

//------------------------------------------------------------------------------------------------
//! Finds the nearest entity with a component. Internal.
class MRX_DebugEntityQuery : Managed
{
	protected vector m_vCenter;
	protected typename m_ComponentType;
	protected IEntity m_Found;
	protected float m_fFoundDistanceSq;

	//------------------------------------------------------------------------------------------------
	void MRX_DebugEntityQuery(vector center, typename componentType)
	{
		m_vCenter = center;
		m_ComponentType = componentType;
	}

	//------------------------------------------------------------------------------------------------
	bool OnEntity(IEntity entity)
	{
		if (!entity.FindComponent(m_ComponentType))
			return true;

		float distanceSq = vector.DistanceSq(m_vCenter, entity.GetOrigin());
		if (!m_Found || distanceSq < m_fFoundDistanceSq)
		{
			m_Found = entity;
			m_fFoundDistanceSq = distanceSq;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	IEntity GetFound()
	{
		return m_Found;
	}
}

//------------------------------------------------------------------------------------------------
//! Player and owner ID, identity, storage backend and economy state.
class MRX_DebugMarxInfo : MRX_DebugAction
{
	//------------------------------------------------------------------------------------------------
	void MRX_DebugMarxInfo()
	{
		Setup("marx.info", "Marx", "Info");
	}

	//------------------------------------------------------------------------------------------------
	override bool HasServerPart()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void RunServer(notnull MRX_DebugContext context, notnull MRX_DebugReply reply)
	{
		MRX_MarxSystem system = MRX_MarxSystem.GetInstance();
		if (!system || !system.GetEconomy() || !system.GetIdentity())
		{
			reply.Done(MRX_DebugResult.Failed("The Marx system is not running (systems config?)"));
			return;
		}

		int playerId = context.m_iPlayerId;
		string ownerId = system.GetIdentity().GetOwnerId(playerId);
		string owner = ownerId;
		if (ownerId.IsEmpty())
			owner = "(none)";

		string note = MRX_DebugRegistry.Get().DescribeOwner(playerId, ownerId);
		if (!note.IsEmpty())
			owner += " (" + note + ")";

		string identity = "resolved";
		if (system.GetIdentity().IsIdentityMissing(playerId))
			identity = "missing (no backend identity)";
		else if (ownerId.IsEmpty())
			identity = "not resolved yet";

		string backend = typename.EnumToString(MRX_EBackendType, system.GetSettings().m_eBackend);
		string economy = typename.EnumToString(MRX_EEconomyServiceState, system.GetEconomy().GetState());
		reply.Done(MRX_DebugResult.Ok(string.Format("player %1, owner %2, identity %3, backend %4, economy %5, stash %6", playerId, owner, identity, backend, economy, system.GetStash() != null)));
	}
}

//------------------------------------------------------------------------------------------------
//! Gives a player without a character (e.g. a PeerTool client in a Game Master scenario, which has no faction slots)
//! a character next to the requesting player.
class MRX_DebugMarxChar : MRX_DebugAction
{
	protected static const ResourceName CHARACTER_PREFAB = "{2F912ED6E399FF47}Prefabs/Characters/Factions/BLUFOR/US_Army/Character_US_Unarmed.et";

	//------------------------------------------------------------------------------------------------
	void MRX_DebugMarxChar()
	{
		Setup("marx.char", "Marx", "Give character");
		AddArg("player");
	}

	//------------------------------------------------------------------------------------------------
	override bool HasServerPart()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void RunServer(notnull MRX_DebugContext context, notnull MRX_DebugReply reply)
	{
		int targetId = context.GetPlayer(0);
		SCR_PlayerController target = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(targetId));
		if (!target)
		{
			reply.Done(MRX_DebugResult.Failed("No such player"));
			return;
		}

		if (target.GetControlledEntity())
		{
			reply.Done(MRX_DebugResult.Failed("That player already has a character"));
			return;
		}

		if (!context.m_Character)
		{
			reply.Done(MRX_DebugResult.Failed("You need a character of your own; the new one appears next to it"));
			return;
		}

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = context.m_Character.GetOrigin() + "1.5 0 0";
		IEntity character = GetGame().SpawnEntityPrefab(Resource.Load(CHARACTER_PREFAB), GetGame().GetWorld(), params);
		if (!character)
		{
			reply.Done(MRX_DebugResult.Failed("Could not spawn the character"));
			return;
		}

		// As the respawn system hands a spawned character to its player.
		target.SetInitialMainEntity(character);
		reply.Done(MRX_DebugResult.Ok(string.Format("Player %1 controls a new character", targetId)));
	}
}

//------------------------------------------------------------------------------------------------
//! Spawns items of a prefab into the requesting player's inventory.
class MRX_DebugMarxItem : MRX_DebugAction
{
	protected static const int MAX_COUNT = 50;

	//------------------------------------------------------------------------------------------------
	void MRX_DebugMarxItem()
	{
		Setup("marx.item", "Marx", "Give item");
		AddArg("prefab");
		AddArg("count", "1", true);
	}

	//------------------------------------------------------------------------------------------------
	override bool HasServerPart()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void RunServer(notnull MRX_DebugContext context, notnull MRX_DebugReply reply)
	{
		CharacterControllerComponent controller = MRX_DebugActionUtils.GetCharacterController(context);
		if (!controller || !controller.GetInventoryStorageManager())
		{
			reply.Done(MRX_DebugActionUtils.NoCharacter());
			return;
		}

		ResourceName prefab = context.GetString(0);
		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid())
		{
			reply.Done(MRX_DebugResult.Failed("No such prefab: " + prefab));
			return;
		}

		int count = context.GetInt(1);
		if (count < 1 || count > MAX_COUNT)
		{
			reply.Done(MRX_DebugResult.Create(MRX_EDebugStatus.BAD_ARGS, string.Format("count must be 1 to %1", MAX_COUNT)));
			return;
		}

		int spawned;
		for (int i = 0; i < count; i++)
		{
			if (controller.GetInventoryStorageManager().TrySpawnPrefabToStorage(prefab))
				spawned++;
		}

		string text = string.Format("%1 of %2 %3 into the inventory", spawned, count, FilePath.StripPath(prefab));
		if (spawned == 0)
			reply.Done(MRX_DebugResult.Failed(text + " (no room?)"));
		else
			reply.Done(MRX_DebugResult.Ok(text));
	}
}

//------------------------------------------------------------------------------------------------
//! Opens the inventory.
class MRX_DebugMarxInventory : MRX_DebugAction
{
	//------------------------------------------------------------------------------------------------
	void MRX_DebugMarxInventory()
	{
		Setup("marx.inv", "Marx", "Inventory");
	}

	//------------------------------------------------------------------------------------------------
	override bool HasClientPart()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override MRX_DebugResult RunClient(notnull MRX_DebugContext context, notnull MRX_DebugResult serverResult)
	{
		CharacterControllerComponent controller = MRX_DebugActionUtils.GetCharacterController(context);
		if (!controller)
			return MRX_DebugActionUtils.NoCharacter();

		SCR_InventoryStorageManagerComponent manager = SCR_InventoryStorageManagerComponent.Cast(controller.GetInventoryStorageManager());
		if (!manager)
			return MRX_DebugResult.Failed("The character has no inventory");

		manager.OpenInventory();
		return MRX_DebugResult.Ok();
	}
}

//------------------------------------------------------------------------------------------------
//! Closes every menu.
class MRX_DebugMarxClose : MRX_DebugAction
{
	//------------------------------------------------------------------------------------------------
	void MRX_DebugMarxClose()
	{
		Setup("marx.close", "Marx", "Close menus");
	}

	//------------------------------------------------------------------------------------------------
	override bool HasClientPart()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override MRX_DebugResult RunClient(notnull MRX_DebugContext context, notnull MRX_DebugResult serverResult)
	{
		GetGame().GetMenuManager().CloseAllMenus();
		return MRX_DebugResult.Ok();
	}
}

//------------------------------------------------------------------------------------------------
//! Opens the pause menu.
class MRX_DebugMarxPause : MRX_DebugAction
{
	//------------------------------------------------------------------------------------------------
	void MRX_DebugMarxPause()
	{
		Setup("marx.pause", "Marx", "Pause menu");
	}

	//------------------------------------------------------------------------------------------------
	override bool HasClientPart()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override MRX_DebugResult RunClient(notnull MRX_DebugContext context, notnull MRX_DebugResult serverResult)
	{
		ArmaReforgerScripted.OpenPauseMenu();
		return MRX_DebugResult.Ok();
	}
}

//------------------------------------------------------------------------------------------------
//! Takes the first weapon (primary first) in hand. On the owning client, which drives the character.
class MRX_DebugMarxEquip : MRX_DebugAction
{
	//------------------------------------------------------------------------------------------------
	void MRX_DebugMarxEquip()
	{
		Setup("marx.equip", "Marx", "Equip weapon");
	}

	//------------------------------------------------------------------------------------------------
	override bool HasClientPart()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override MRX_DebugResult RunClient(notnull MRX_DebugContext context, notnull MRX_DebugResult serverResult)
	{
		ChimeraCharacter character = context.GetChimera();
		if (!character || !character.GetCharacterController())
			return MRX_DebugActionUtils.NoCharacter();

		BaseWeaponManagerComponent weaponManager = BaseWeaponManagerComponent.Cast(character.FindComponent(BaseWeaponManagerComponent));
		if (!weaponManager)
			return MRX_DebugResult.Failed("The character has no weapon manager");

		array<WeaponSlotComponent> slots = {};
		weaponManager.GetWeaponsSlots(slots);
		foreach (WeaponSlotComponent slot : slots)
		{
			IEntity weapon = slot.GetWeaponEntity();
			if (!weapon)
				continue;

			character.GetCharacterController().TryEquipRightHandItem(weapon, EEquipItemType.EEquipTypeWeapon, false);
			return MRX_DebugResult.Ok(MRX_DebugActionUtils.GetPrefabFileName(weapon));
		}

		return MRX_DebugResult.Failed("No weapon");
	}
}

//------------------------------------------------------------------------------------------------
//! What the character holds, as the server sees it.
class MRX_DebugMarxHands : MRX_DebugAction
{
	//------------------------------------------------------------------------------------------------
	void MRX_DebugMarxHands()
	{
		Setup("marx.hands", "Marx", "Hands");
	}

	//------------------------------------------------------------------------------------------------
	override bool HasServerPart()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void RunServer(notnull MRX_DebugContext context, notnull MRX_DebugReply reply)
	{
		CharacterControllerComponent controller = MRX_DebugActionUtils.GetCharacterController(context);
		if (!controller)
		{
			reply.Done(MRX_DebugActionUtils.NoCharacter());
			return;
		}

		string weapon;
		BaseWeaponManagerComponent weaponManager = controller.GetWeaponManagerComponent();
		if (weaponManager && weaponManager.GetCurrentWeapon())
			weapon = MRX_DebugActionUtils.GetPrefabFileName(weaponManager.GetCurrentWeapon().GetOwner());

		string item = MRX_DebugActionUtils.GetPrefabFileName(controller.GetCurrentItemInHands());
		reply.Done(MRX_DebugResult.Ok(string.Format("item=%1 weapon=%2 changing=%3 raised=%4 canFire=%5 third=%6", item, weapon, controller.IsChangingItem(), controller.IsWeaponRaised(), controller.GetCanFireWeapon(), controller.IsInThirdPersonView())));
	}
}

//------------------------------------------------------------------------------------------------
//! The character's carried items, as the server sees them: prefab, issued (MRX_IssuedItems), rounds of magazines.
class MRX_DebugMarxGear : MRX_DebugAction
{
	//------------------------------------------------------------------------------------------------
	void MRX_DebugMarxGear()
	{
		Setup("marx.gear", "Marx", "Gear");
	}

	//------------------------------------------------------------------------------------------------
	override bool HasServerPart()
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

		array<IEntity> items = {};
		MRX_EntitySnapshots.GetLoadoutItems(context.m_Character, items);
		string text = string.Format("%1 items", items.Count());
		foreach (IEntity item : items)
		{
			string rounds;
			BaseMagazineComponent magazine = BaseMagazineComponent.Cast(item.FindComponent(BaseMagazineComponent));
			if (magazine)
				rounds = string.Format(" rounds %1/%2", magazine.GetAmmoCount(), magazine.GetMaxAmmoCount());

			text += string.Format("\n%1 issued=%2%3", MRX_DebugActionUtils.GetPrefabFileName(item), MRX_IssuedItems.IsIssued(item), rounds);
		}

		reply.Done(MRX_DebugResult.Ok(text));
	}
}

//------------------------------------------------------------------------------------------------
//! Switches between first and third person.
class MRX_DebugMarxView : MRX_DebugAction
{
	//------------------------------------------------------------------------------------------------
	void MRX_DebugMarxView()
	{
		Setup("marx.view", "Marx", "1st/3rd person");
	}

	//------------------------------------------------------------------------------------------------
	override bool HasClientPart()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override MRX_DebugResult RunClient(notnull MRX_DebugContext context, notnull MRX_DebugResult serverResult)
	{
		CharacterControllerComponent controller = MRX_DebugActionUtils.GetCharacterController(context);
		if (!controller)
			return MRX_DebugActionUtils.NoCharacter();

		controller.SetInThirdPersonView(!controller.IsInThirdPersonView());
		return MRX_DebugResult.Ok(string.Format("third person %1", controller.IsInThirdPersonView()));
	}
}

//------------------------------------------------------------------------------------------------
//! Kills the requesting player's character, e.g. to try respawning.
class MRX_DebugMarxKill : MRX_DebugAction
{
	//------------------------------------------------------------------------------------------------
	void MRX_DebugMarxKill()
	{
		Setup("marx.kill", "Marx", "Kill me");
	}

	//------------------------------------------------------------------------------------------------
	override bool HasServerPart()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void RunServer(notnull MRX_DebugContext context, notnull MRX_DebugReply reply)
	{
		CharacterControllerComponent controller = MRX_DebugActionUtils.GetCharacterController(context);
		if (!controller)
		{
			reply.Done(MRX_DebugActionUtils.NoCharacter());
			return;
		}

		controller.ForceDeath();
		reply.Done(MRX_DebugResult.Ok());
	}
}
