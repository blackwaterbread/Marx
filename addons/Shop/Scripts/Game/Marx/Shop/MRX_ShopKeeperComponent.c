[ComponentEditorProps(category: "Marx", description: "Makes a character a shop keeper that takes no damage. Use it with MRX_ShopComponent and MRX_OpenShopAction on a character prefab placed without AI.")]
class MRX_ShopKeeperComponentClass : ScriptComponentClass
{
}

//! Shop keeper (API v0): a character that sells, e.g. with MRX_ShopComponent and MRX_OpenShopAction in its
//! ActionsManagerComponent. It takes no damage, so the shop stays open. A character placed without a group has no AI
//! and just stands. Server.
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
		if (!GetGame().InPlayMode() || !Replication.IsServer())
			return;

		DamageManagerComponent damageManager = DamageManagerComponent.Cast(owner.FindComponent(DamageManagerComponent));
		if (damageManager)
			damageManager.EnableDamageHandling(false);
	}
}
