#ifdef ENABLE_DIAG
//! Diag builds only: the debug panel's diag menu entries and drawing, and (Workbench only) the debug file commands.
modded class ArmaReforgerScripted
{
	//------------------------------------------------------------------------------------------------
	override bool OnGameStart()
	{
		bool result = super.OnGameStart();
		MRX_DebugPanel.Register();
		return result;
	}

	//------------------------------------------------------------------------------------------------
	override void OnUpdate(BaseWorld world, float timeslice)
	{
		super.OnUpdate(world, timeslice);
		MRX_DebugPanel.Update();

#ifdef WORKBENCH
		MRX_DebugFileCommands.Update();
#endif
	}
}
#endif
