//! "Stash" interaction (API v0): asks the server to open the player's stash, which then shows in the vanilla inventory.
//! Add it to the ActionsManagerComponent of an entity that has MRX_StashPointComponent and an enabled RplComponent.
class MRX_OpenStashAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (pUserEntity != SCR_PlayerController.GetLocalControlledEntity())
			return;

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (controller && pOwnerEntity.FindComponent(MRX_StashPointComponent))
			controller.MRX_RequestStashOpen(pOwnerEntity);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return GetOwner().FindComponent(MRX_StashPointComponent) != null;
	}

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBroadcastScript()
	{
		return false;
	}
}
