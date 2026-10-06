[ComponentEditorProps(category: "Marx", description: "Makes a character a shop keeper that takes no damage. Use it with MRX_ShopComponent and MRX_OpenShopAction on a character prefab placed without AI.")]
class MRX_ShopKeeperComponentClass : ScriptComponentClass
{
}

//! Shop keeper (API v0): a character that sells, e.g. with MRX_ShopComponent and MRX_OpenShopAction in its
//! ActionsManagerComponent. It takes no damage, so the shop stays open. A character placed without a group has no AI
//! and just stands. A character inside another entity (e.g. placed in a composition prefab) keeps playing its falling
//! animation, so the keeper detaches itself from its parent, keeping its position.
class MRX_ShopKeeperComponent : ScriptComponent
{
	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		SetEventMask(owner, EntityEvent.INIT);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);
		if (!GetGame().InPlayMode())
			return;

		// Every machine: the parent is created with its children everywhere.
		if (owner.GetParent())
			GetGame().GetCallqueue().Call(DetachFromParent, owner);

		if (!Replication.IsServer())
			return;

		DamageManagerComponent damageManager = DamageManagerComponent.Cast(owner.FindComponent(DamageManagerComponent));
		if (damageManager)
			damageManager.EnableDamageHandling(false);
	}

	//------------------------------------------------------------------------------------------------
	protected static void DetachFromParent(IEntity owner)
	{
		if (!owner || !owner.GetParent())
			return;

		vector transform[4];
		owner.GetWorldTransform(transform);
		owner.GetParent().RemoveChild(owner, true);

		// A character keeps the heading it was placed with in its parent: teleport it to where it stood.
		BaseGameEntity gameEntity = BaseGameEntity.Cast(owner);
		if (gameEntity)
			gameEntity.Teleport(transform);
	}
}
