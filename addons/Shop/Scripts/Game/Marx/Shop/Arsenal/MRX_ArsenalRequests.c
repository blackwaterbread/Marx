//! Server checks of Marx arsenal requests (API v0), the same as vanilla arsenal requests: an alive character within
//! 30 m of the arsenal that it may interact with, and a target storage of its own (or a nearby one of nobody's).
class MRX_ArsenalRequests
{
	//------------------------------------------------------------------------------------------------
	//! Checks a purchase into storage and starts it. The callback gets the result unless a status other than OK is
	//! returned.
	static MRX_EShopStatus Buy(int playerId, IEntity arsenalEntity, BaseInventoryStorageComponent storage, string itemId, MRX_ShopCallback callback)
	{
		MRX_ShopDefinition shop;
		IEntity character;
		MRX_EShopStatus status = Resolve(playerId, arsenalEntity, shop, character);
		if (status != MRX_EShopStatus.OK)
			return status;

		if (!storage)
			return MRX_EShopStatus.NO_SPACE;

		status = CheckTargetStorage(character, arsenalEntity, storage);
		if (status != MRX_EShopStatus.OK)
			return status;

		MRX_ShopStorageTarget target = MRX_ShopStorageTarget.Create(storage);
		if (!target)
			return MRX_EShopStatus.NO_SPACE;

		MRX_Shop.GetService().Buy(playerId, shop, itemId, callback, target);
		return MRX_EShopStatus.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Checks a sale of an item with its contents and starts it. The callback gets the result unless a status other than
	//! OK is returned.
	static MRX_EShopStatus Sell(int playerId, IEntity arsenalEntity, IEntity item, MRX_ShopCallback callback)
	{
		MRX_ShopDefinition shop;
		IEntity character;
		MRX_EShopStatus status = Resolve(playerId, arsenalEntity, shop, character);
		if (status != MRX_EShopStatus.OK)
			return status;

		// The shop service checks that the item is carried by the player's character.
		MRX_Shop.GetService().Sell(playerId, shop, item, callback, true);
		return MRX_EShopStatus.OK;
	}

	//------------------------------------------------------------------------------------------------
	protected static MRX_EShopStatus Resolve(int playerId, IEntity arsenalEntity, out MRX_ShopDefinition shop, out IEntity character)
	{
		if (!MRX_Shop.GetService())
			return MRX_EShopStatus.UNKNOWN_SHOP;

		MRX_ArsenalShopComponent arsenal = MRX_ArsenalShopComponent.Find(arsenalEntity);
		if (!arsenal)
			return MRX_EShopStatus.UNKNOWN_SHOP;

		shop = arsenal.GetShop();
		if (!shop)
			return MRX_EShopStatus.UNKNOWN_SHOP;

		ChimeraCharacter chimera = ChimeraCharacter.Cast(GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId));
		if (!chimera || !chimera.GetCharacterController() || chimera.GetCharacterController().GetLifeState() != ECharacterLifeState.ALIVE)
			return MRX_EShopStatus.TOO_FAR;

		character = chimera;
		float maxDistance = MRX_ArsenalShopComponent.MAX_DISTANCE;
		if (vector.DistanceSq(character.GetOrigin(), arsenalEntity.GetOrigin()) > maxDistance * maxDistance)
			return MRX_EShopStatus.TOO_FAR;

		// Spatial check of the vanilla arsenal requests.
		InventoryStorageManagerComponent manager = chimera.GetCharacterController().GetInventoryStorageManager();
		if (!manager || !manager.ValidateStorageRequest(arsenalEntity))
			return MRX_EShopStatus.TOO_FAR;

		return MRX_EShopStatus.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! The storage is the character's own, or nobody's and near both the character and the arsenal; never an arsenal.
	protected static MRX_EShopStatus CheckTargetStorage(notnull IEntity character, notnull IEntity arsenalEntity, notnull BaseInventoryStorageComponent storage)
	{
		IEntity owner = storage.GetOwner();
		if (!owner || SCR_ArsenalComponent.FindArsenalComponent(owner, false))
			return MRX_EShopStatus.REJECTED;

		IEntity parent = owner;
		while (parent)
		{
			if (ChimeraCharacter.Cast(parent))
			{
				if (parent != character)
					return MRX_EShopStatus.REJECTED;

				return MRX_EShopStatus.OK;
			}

			parent = parent.GetParent();
		}

		float maxDistance = MRX_ArsenalShopComponent.MAX_DISTANCE;
		if (vector.DistanceSq(character.GetOrigin(), owner.GetOrigin()) > maxDistance * maxDistance)
			return MRX_EShopStatus.TOO_FAR;

		if (vector.DistanceSq(arsenalEntity.GetOrigin(), owner.GetOrigin()) > maxDistance * maxDistance)
			return MRX_EShopStatus.TOO_FAR;

		return MRX_EShopStatus.OK;
	}
}
