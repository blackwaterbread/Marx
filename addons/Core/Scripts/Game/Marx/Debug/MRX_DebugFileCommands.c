#ifdef WORKBENCH
//! Workbench only: runs debug actions from a file, for automation that cannot type in the game view. Every half second
//! in Play mode, the host reads one line "<action ID> [values...]" from $profile:mrx_cmd.txt, deletes the file and runs
//! the action as the debug panel does (MRX_DebugRunner.Request). Results are logged as "[MRX_DBG] ...".
class MRX_DebugFileCommands
{
	static const string FILE = "$profile:mrx_cmd.txt";
	protected static const int POLL_MS = 500;

	protected static int s_iLastPollTick;

	//------------------------------------------------------------------------------------------------
	//! Every frame (ArmaReforgerScripted.OnUpdate): checks the file every POLL_MS on a server with a local player.
	static void Update()
	{
		int now = System.GetTickCount();
		if (now - s_iLastPollTick < POLL_MS)
			return;

		s_iLastPollTick = now;
		if (!Replication.IsServer() || !GetGame().GetPlayerController() || !FileIO.FileExists(FILE))
			return;

		FileHandle file = FileIO.OpenFile(FILE, FileMode.READ);
		string line;
		if (file)
		{
			file.ReadLine(line);
			file.Close();
		}

		FileIO.DeleteFile(FILE);
		line = line.Trim();
		array<string> words = {};
		line.Split(" ", words, true);
		if (words.IsEmpty())
			return;

		Print(MRX_DebugRunner.LOG_TAG + "file command: " + line);
		string actionId = words[0];
		words.RemoveOrdered(0);
		MRX_DebugRunner.Request(actionId, words);
	}
}
#endif
