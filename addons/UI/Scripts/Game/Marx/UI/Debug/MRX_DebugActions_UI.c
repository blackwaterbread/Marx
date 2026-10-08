//! Debug actions of the "UI" page: language and sample windows of the shared Marx UI.

//------------------------------------------------------------------------------------------------
modded class MRX_DebugRegistry
{
	//------------------------------------------------------------------------------------------------
	override protected void RegisterActions()
	{
		super.RegisterActions();
		Register(new MRX_DebugUiLang());
		Register(new MRX_DebugUiDialog());
		Register(new MRX_DebugUiGallery());
	}
}

//------------------------------------------------------------------------------------------------
//! Switches the UI language, e.g. to check the Korean texts.
class MRX_DebugUiLang : MRX_DebugAction
{
	//------------------------------------------------------------------------------------------------
	void MRX_DebugUiLang()
	{
		Setup("ui.lang", "UI", "Language");
		AddArg("code", "ko_kr");
	}

	//------------------------------------------------------------------------------------------------
	override bool HasClientPart()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override MRX_DebugResult RunClient(notnull MRX_DebugContext context, notnull MRX_DebugResult serverResult)
	{
		WidgetManager.SetLanguage(context.GetString(0));
		return MRX_DebugResult.Ok(context.GetString(0));
	}
}

//------------------------------------------------------------------------------------------------
//! Opens an MRX_ScriptedDialog with sample content (MRX_DebugSampleDialog.KINDS).
class MRX_DebugUiDialog : MRX_DebugAction
{
	//------------------------------------------------------------------------------------------------
	void MRX_DebugUiDialog()
	{
		Setup("ui.dialog", "UI", "Sample dialog");
		AddArg("kind", MRX_DebugSampleDialog.KIND_BASIC);
	}

	//------------------------------------------------------------------------------------------------
	override bool HasClientPart()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override MRX_DebugResult RunClient(notnull MRX_DebugContext context, notnull MRX_DebugResult serverResult)
	{
		string kind = context.GetString(0);
		if (!MRX_DebugSampleDialog.IsKind(kind))
			return MRX_DebugResult.Create(MRX_EDebugStatus.BAD_ARGS, string.Format("Unknown kind '%1'. Kinds: %2", kind, MRX_DebugSampleDialog.KINDS));

		MRX_DebugSampleDialog.Open(kind);
		return MRX_DebugResult.Ok(kind);
	}
}

//------------------------------------------------------------------------------------------------
//! Opens the Marx widget gallery (MRX_DebugGalleryDialog).
class MRX_DebugUiGallery : MRX_DebugAction
{
	//------------------------------------------------------------------------------------------------
	void MRX_DebugUiGallery()
	{
		Setup("ui.gallery", "UI", "Widget gallery");
	}

	//------------------------------------------------------------------------------------------------
	override bool HasClientPart()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override MRX_DebugResult RunClient(notnull MRX_DebugContext context, notnull MRX_DebugResult serverResult)
	{
		MRX_DebugGalleryDialog.Open();
		return MRX_DebugResult.Ok();
	}
}
