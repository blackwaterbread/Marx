#ifdef WORKBENCH
// Tests of the admin command logic (parsing, target lookup, economy calls, PENDING polling) on the running Marx system.
// The engine's chat routing and permission checks are not covered.

//------------------------------------------------------------------------------------------------
class MRX_AdminCommandTests
{
	//------------------------------------------------------------------------------------------------
	static void Register(notnull MRX_TestRunner runner)
	{
		runner.Add(new MRX_Test_AdminCommand());
	}
}

//------------------------------------------------------------------------------------------------
//! Exposes the protected execution entry points. Its own keyword keeps it apart from the real command.
class MRX_TestAdminCommand : MRX_AdminCommand
{
	//------------------------------------------------------------------------------------------------
	override string GetKeyword()
	{
		return "marxtest";
	}

	//------------------------------------------------------------------------------------------------
	ScrServerCmdResult Execute(array<string> argv, int executorId)
	{
		return OnChatServerExecution(argv, executorId);
	}

	//------------------------------------------------------------------------------------------------
	ScrServerCmdResult Poll()
	{
		return OnUpdate();
	}
}

//------------------------------------------------------------------------------------------------
class MRX_AdminStep : Managed
{
	ref array<string> m_aArgs;
	EServerCmdResultType m_eExpected;
	//! Must appear in the response; empty = not checked.
	string m_sExpectedText;
	//! Checks the cached cash balance against the start balance plus this delta after the step.
	bool m_bCheckBalance;
	int m_iBalanceDelta;

	//------------------------------------------------------------------------------------------------
	MRX_AdminStep CheckBalance(int delta)
	{
		m_bCheckBalance = true;
		m_iBalanceDelta = delta;
		return this;
	}
}

//------------------------------------------------------------------------------------------------
//! Help, argument errors, balance (also the executor's own), give and take (by ID and by quoted name), refused take; balance ends unchanged.
class MRX_Test_AdminCommand : MRX_TestCase
{
	protected static const int POLL_MS = 100;
	protected static const int POLL_LIMIT_MS = 12000;

	protected ref MRX_TestAdminCommand m_Command = new MRX_TestAdminCommand();
	protected ref array<ref MRX_AdminStep> m_aSteps = {};
	protected int m_iStep = -1;
	protected int m_iPlayerId;
	protected string m_sOwnerId;
	protected int m_iStartBalance;
	protected int m_iPolledMs;

	//------------------------------------------------------------------------------------------------
	override int GetTimeoutMs()
	{
		return 30000;
	}

	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		PlayerController controller = GetGame().GetPlayerController();
		MRX_EconomyService economy = MRX_Marx.GetEconomy();
		if (!controller || !economy)
		{
			Skip("no local player or no economy");
			return;
		}

		m_iPlayerId = controller.GetPlayerId();
		m_sOwnerId = MRX_Marx.GetOwnerId(m_iPlayerId);
		if (m_sOwnerId.IsEmpty() || !economy.TryGetCachedBalance(m_sOwnerId, MRX_Settings.DEFAULT_CURRENCY, m_iStartBalance))
		{
			Skip("the local player has no owner or no loaded wallet (start with -" + MRX_TestRunner.TEST_IDENTITY_PARAM + ")");
			return;
		}

		string id = m_iPlayerId.ToString();
		string quotedName = "\"" + GetGame().GetPlayerManager().GetPlayerName(m_iPlayerId) + "\"";

		AddStep({"marx"}, EServerCmdResultType.OK, "#marx give");
		AddStep({"marx", "fly", id}, EServerCmdResultType.OK, "#marx give");
		AddStep({"marx", "give", "999", "5"}, EServerCmdResultType.ERR, "Player not found");
		AddStep({"marx", "give", id, "abc"}, EServerCmdResultType.PARAMETERS, "Invalid amount");
		AddStep({"marx", "give", id, "0"}, EServerCmdResultType.PARAMETERS, "Invalid amount");
		AddStep({"marx", "give", id, "1234567890"}, EServerCmdResultType.PARAMETERS, "Invalid amount");
		AddStep({"marx", "give", id, "5", "gold"}, EServerCmdResultType.PARAMETERS, "Unknown currency");
		AddStep({"marx", "balance", id}, EServerCmdResultType.OK, string.Format(": %1 cash", m_iStartBalance));
		AddStep({"marx", "balance"}, EServerCmdResultType.OK, string.Format("[%1]: %2 cash", id, m_iStartBalance));
		AddStep({"marx", "give", id, "15"}, EServerCmdResultType.OK, "give 15 cash").CheckBalance(15);
		AddStep({"marx", "balance", quotedName, "cash"}, EServerCmdResultType.OK, string.Format(": %1 cash", m_iStartBalance + 15));
		AddStep({"marx", "take", quotedName, "15"}, EServerCmdResultType.OK, "take 15 cash").CheckBalance(0);
		AddStep({"marx", "take", id, "999999999"}, EServerCmdResultType.ERR, "INSUFFICIENT_FUNDS").CheckBalance(0);

		NextStep();
	}

	//------------------------------------------------------------------------------------------------
	protected MRX_AdminStep AddStep(array<string> args, EServerCmdResultType expected, string expectedText)
	{
		MRX_AdminStep step = new MRX_AdminStep();
		step.m_aArgs = args;
		step.m_eExpected = expected;
		step.m_sExpectedText = expectedText;
		m_aSteps.Insert(step);
		return step;
	}

	//------------------------------------------------------------------------------------------------
	protected void NextStep()
	{
		m_iStep++;
		if (m_iStep >= m_aSteps.Count())
		{
			Finish();
			return;
		}

		m_iPolledMs = 0;
		HandleResult(m_Command.Execute(m_aSteps[m_iStep].m_aArgs, m_iPlayerId));
	}

	//------------------------------------------------------------------------------------------------
	protected void PollStep()
	{
		m_iPolledMs += POLL_MS;
		HandleResult(m_Command.Poll());
	}

	//------------------------------------------------------------------------------------------------
	protected void HandleResult(ScrServerCmdResult result)
	{
		if (result && result.m_eResultType == EServerCmdResultType.PENDING && m_iPolledMs < POLL_LIMIT_MS)
		{
			GetGame().GetCallqueue().CallLater(PollStep, POLL_MS);
			return;
		}

		MRX_AdminStep step = m_aSteps[m_iStep];
		string label = string.Format("step %1 (%2)", m_iStep, SCR_StringHelper.Join(" ", step.m_aArgs));
		if (!result)
		{
			Check(false, label + ": no result");
			NextStep();
			return;
		}

		if (result.m_eResultType != step.m_eExpected)
			Check(false, string.Format("%1: expected %2, got %3 '%4'", label, typename.EnumToString(EServerCmdResultType, step.m_eExpected), typename.EnumToString(EServerCmdResultType, result.m_eResultType), result.m_sResponse));

		if (!step.m_sExpectedText.IsEmpty() && !result.m_sResponse.Contains(step.m_sExpectedText))
			Check(false, string.Format("%1: response '%2' lacks '%3'", label, result.m_sResponse, step.m_sExpectedText));

		if (step.m_bCheckBalance)
		{
			int balance;
			MRX_Marx.GetEconomy().TryGetCachedBalance(m_sOwnerId, MRX_Settings.DEFAULT_CURRENCY, balance);
			CheckInt(balance, m_iStartBalance + step.m_iBalanceDelta, label + " balance");
		}

		NextStep();
	}
}
#endif
