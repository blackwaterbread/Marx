//! "Stash" interaction (API v0): opens the stash UI on the client of the player who uses it.
//! Add it to the ActionsManagerComponent of an entity that has MRX_StashPointComponent.
class MRX_OpenStashAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (pUserEntity != SCR_PlayerController.GetLocalControlledEntity())
			return;

		MRX_StashPointComponent stashPoint = MRX_StashPointComponent.Cast(pOwnerEntity.FindComponent(MRX_StashPointComponent));
		if (stashPoint)
			MRX_StashMenu.Open(stashPoint);
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
