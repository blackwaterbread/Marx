//! No interactions (and no interaction prompt) while a Marx dialog is open. Vanilla only stops them for menus, not for
//! dialogs, so the prompt of the object that opened the dialog stayed on top of it and could open it again.
modded class SCR_InteractionHandlerComponent
{
	//------------------------------------------------------------------------------------------------
	override protected bool GetCanInteractScript(IEntity controlledEntity)
	{
		if (MRX_ScriptedDialog.IsAnyOpen())
			return false;

		return super.GetCanInteractScript(controlledEntity);
	}
}
