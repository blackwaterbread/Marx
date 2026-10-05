#ifdef WORKBENCH
//! World helpers for the entity tests (server = host in Workbench Play).
class MRX_TestWorldUtils
{
	static const ResourceName CHARACTER_PREFAB = "{2F912ED6E399FF47}Prefabs/Characters/Factions/BLUFOR/US_Army/Character_US_Unarmed.et";

	//------------------------------------------------------------------------------------------------
	//! Below the current (Game Master) camera.
	static vector GetCameraPosition()
	{
		vector camera[4];
		GetGame().GetWorld().GetCurrentCamera(camera);
		return camera[3];
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns on the ground at the position's x/z.
	static IEntity SpawnPrefab(ResourceName prefab, vector position)
	{
		position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]);
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = position;
		return GetGame().SpawnEntityPrefab(Resource.Load(prefab), GetGame().GetWorld(), params);
	}
}

//------------------------------------------------------------------------------------------------
//! Gives the local player a character for a test: the controlled one, or a spawned one it possesses until Release().
class MRX_TestPossession : Managed
{
	protected SCR_PlayerController m_Controller;
	protected IEntity m_Character;
	protected bool m_bPossessing;

	//------------------------------------------------------------------------------------------------
	//! \return Null when there is no local player controller.
	static MRX_TestPossession Acquire()
	{
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
			return null;

		MRX_TestPossession possession = new MRX_TestPossession();
		possession.m_Controller = controller;
		possession.m_Character = controller.GetControlledEntity();
		if (!possession.m_Character)
		{
			possession.m_Character = MRX_TestWorldUtils.SpawnPrefab(MRX_TestWorldUtils.CHARACTER_PREFAB, MRX_TestWorldUtils.GetCameraPosition());
			if (possession.m_Character)
			{
				possession.m_bPossessing = true;
				controller.SetPossessedEntity(possession.m_Character);
			}
		}

		return possession;
	}

	//------------------------------------------------------------------------------------------------
	SCR_PlayerController GetController()
	{
		return m_Controller;
	}

	//------------------------------------------------------------------------------------------------
	IEntity GetCharacter()
	{
		return m_Character;
	}

	//------------------------------------------------------------------------------------------------
	//! Items carried by the character, flat.
	int GetItems(notnull array<IEntity> outItems)
	{
		outItems.Clear();
		ChimeraCharacter character = ChimeraCharacter.Cast(m_Character);
		if (!character || !character.GetCharacterController())
			return 0;

		InventoryStorageManagerComponent manager = character.GetCharacterController().GetInventoryStorageManager();
		if (manager)
			manager.GetItems(outItems);

		return outItems.Count();
	}

	//------------------------------------------------------------------------------------------------
	void Release()
	{
		if (!m_bPossessing || !m_Controller)
			return;

		m_bPossessing = false;
		m_Controller.SetPossessedEntity(null);
		if (m_Character)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Character);
	}
}
#endif
