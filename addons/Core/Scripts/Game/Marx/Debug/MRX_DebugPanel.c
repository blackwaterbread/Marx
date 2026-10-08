#ifdef ENABLE_DIAG
//! Diag builds only: "Marx > Debug panel" in the diag menu shows a DbgUI window that runs debug actions
//! (MRX_DebugRegistry) for the local player: one page per area, a button and value fields per action, the player's
//! state and the latest results. English only, it is a development tool.

modded enum SCR_DebugMenuID
{
	MRX_DEBUGUI_MENU,
	MRX_DEBUGUI_PANEL
}

//------------------------------------------------------------------------------------------------
class MRX_DebugPanel : Managed
{
	protected static const string TITLE = "Marx debug";
	protected static const int FIELD_WIDTH = 110;
	//! Older results show their first line only, up to this length.
	protected static const int SHORT_TEXT_LENGTH = 100;

	protected static ref MRX_DebugPanel s_Instance;

	protected string m_sPage;
	//! Field values by "<action ID>/<argument name>"; empty fields use the argument's default.
	protected ref map<string, string> m_mValues = new map<string, string>();

	//------------------------------------------------------------------------------------------------
	//! Adds the diag menu entries. Called on every game start, as the vanilla entries are.
	static void Register()
	{
		DiagMenu.RegisterMenu(SCR_DebugMenuID.MRX_DEBUGUI_MENU, "Marx", "");
		DiagMenu.RegisterBool(SCR_DebugMenuID.MRX_DEBUGUI_PANEL, "", "Debug panel", "Marx");
	}

	//------------------------------------------------------------------------------------------------
	//! Every frame: draws the panel while its diag menu entry is on.
	static void Update()
	{
		if (!DiagMenu.GetBool(SCR_DebugMenuID.MRX_DEBUGUI_PANEL))
			return;

		if (!s_Instance)
			s_Instance = new MRX_DebugPanel();

		s_Instance.Draw();
	}

	//------------------------------------------------------------------------------------------------
	protected void Draw()
	{
		MRX_DebugRegistry registry = MRX_DebugRegistry.Get();
		array<string> pages = {};
		registry.GetPages(pages);
		if (!pages.Contains(m_sPage) && !pages.IsEmpty())
			m_sPage = pages[0];

		DbgUI.Begin(TITLE, 20, 120);
		DrawPages(pages);
		DbgUI.Spacer(4);
		DrawState(registry);
		DbgUI.Spacer(4);
		DrawActions(registry);
		DbgUI.Spacer(4);
		DrawResults();
		DbgUI.End();
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawPages(notnull array<string> pages)
	{
		DbgUI.PushID("pages");
		foreach (int i, string page : pages)
		{
			if (i > 0)
				DbgUI.SameLine();

			string label = page;
			if (page == m_sPage)
				label = "[" + page + "]";

			if (DbgUI.Button(label))
				m_sPage = page;
		}

		DbgUI.PopID();
	}

	//------------------------------------------------------------------------------------------------
	//! Player, owner (known on the server only), wallet as the client sees it, and requests waiting for the server.
	protected void DrawState(notnull MRX_DebugRegistry registry)
	{
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
		{
			DbgUI.Text("No local player");
			return;
		}

		int playerId = controller.GetPlayerId();
		string owner = "(on the server only, see marx.info)";
		if (Replication.IsServer())
		{
			string ownerId = MRX_Marx.GetOwnerId(playerId);
			owner = ownerId;
			if (ownerId.IsEmpty())
				owner = "(none)";

			string note = registry.DescribeOwner(playerId, ownerId);
			if (!note.IsEmpty())
				owner += " (" + note + ")";
		}

		DbgUI.Text(string.Format("Player %1, owner %2", playerId, owner));

		MRX_ClientWallet wallet = controller.MRX_GetWallet();
		array<string> currencies = {};
		if (wallet.GetCurrencies(currencies) == 0)
			DbgUI.Text("Wallet: nothing from the server yet");

		foreach (string currency : currencies)
		{
			int balance;
			wallet.TryGetBalance(currency, balance);
			DbgUI.Text(string.Format("Wallet %1: %2", currency, balance));
		}

		int pending = MRX_DebugRunner.GetPendingCount();
		if (pending > 0)
			DbgUI.Text(string.Format("Waiting for %1 request(s)", pending));
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawActions(notnull MRX_DebugRegistry registry)
	{
		array<MRX_DebugAction> actions = {};
		registry.GetActions(m_sPage, actions);
		array<ref MRX_DebugArg> args = {};
		foreach (MRX_DebugAction action : actions)
		{
			string actionId = action.GetId();
			action.GetArgs(args);
			DbgUI.PushID(actionId);

			// Values as the fields showed them last frame; empty ones take the default when the action runs.
			array<string> values = {};
			foreach (MRX_DebugArg arg : args)
			{
				values.Insert(GetValue(actionId, arg));
			}

			bool run = DbgUI.Button(action.GetLabel(), 120);
			foreach (MRX_DebugArg arg : args)
			{
				string value = GetValue(actionId, arg);
				DbgUI.SameLine();
				DbgUI.InputText(arg.m_sName, value, FIELD_WIDTH);
				m_mValues.Set(GetKey(actionId, arg), value);
			}

			DbgUI.PopID();
			if (run)
				MRX_DebugRunner.Request(actionId, values);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Newest first; the newest in full, older ones shortened.
	protected void DrawResults()
	{
		array<ref MRX_DebugRecord> recent = MRX_DebugRunner.GetRecent();
		if (recent.IsEmpty())
		{
			DbgUI.Text("No results yet");
			return;
		}

		for (int i = recent.Count() - 1; i >= 0; i--)
		{
			MRX_DebugRecord record = recent[i];
			string text = record.m_sText;
			if (i < recent.Count() - 1)
				text = Shorten(text);

			DbgUI.Text(string.Format("%1 %2 %3 %4", record.GetTimeText(), record.m_sActionId, record.GetStatusText(), text));
		}
	}

	//------------------------------------------------------------------------------------------------
	protected string GetValue(string actionId, notnull MRX_DebugArg arg)
	{
		string value;
		if (!m_mValues.Find(GetKey(actionId, arg), value))
			value = arg.m_sDefault;

		return value;
	}

	//------------------------------------------------------------------------------------------------
	protected static string GetKey(string actionId, notnull MRX_DebugArg arg)
	{
		return actionId + "/" + arg.m_sName;
	}

	//------------------------------------------------------------------------------------------------
	protected static string Shorten(string text)
	{
		int lineEnd = text.IndexOf("\n");
		if (lineEnd >= 0)
			text = text.Substring(0, lineEnd) + " ...";

		if (text.Length() > SHORT_TEXT_LENGTH)
			text = text.Substring(0, SHORT_TEXT_LENGTH) + "...";

		return text;
	}
}
#endif
