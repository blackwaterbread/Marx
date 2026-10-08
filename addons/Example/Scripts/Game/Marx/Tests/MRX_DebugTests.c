#ifdef WORKBENCH
// Tests of the debug action plumbing: argument values, registration and the server run of an action.

//------------------------------------------------------------------------------------------------
class MRX_DebugTests
{
	//------------------------------------------------------------------------------------------------
	static void Register(notnull MRX_TestRunner runner)
	{
		runner.Add(new MRX_Test_DebugArgs());
		runner.Add(new MRX_Test_DebugRegistry());
		runner.Add(new MRX_Test_DebugExecute());
	}
}

//------------------------------------------------------------------------------------------------
//! Client part only, with a required whole number, an optional text and an optional whole number.
class MRX_TestDebugArgsAction : MRX_DebugAction
{
	//------------------------------------------------------------------------------------------------
	void MRX_TestDebugArgsAction()
	{
		Setup("test.args", "Test", "Args");
		AddArg("amount", "", true);
		AddArg("currency", "default");
		AddArg("count", "1", true);
	}
}

//------------------------------------------------------------------------------------------------
//! Server part that keeps the reply and answers when the test says so.
class MRX_TestDebugLateAction : MRX_DebugAction
{
	ref MRX_DebugReply m_Reply;
	int m_iRuns;

	//------------------------------------------------------------------------------------------------
	void MRX_TestDebugLateAction()
	{
		Setup("test.late", "Test", "Late");
		AddArg("slot", "0", true);
	}

	//------------------------------------------------------------------------------------------------
	override bool HasServerPart()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void RunServer(notnull MRX_DebugContext context, notnull MRX_DebugReply reply)
	{
		m_iRuns++;
		m_Reply = reply;
	}
}

//------------------------------------------------------------------------------------------------
class MRX_TestDebugCountingReply : MRX_DebugReply
{
	int m_iDelivered;

	//------------------------------------------------------------------------------------------------
	override protected void Deliver(notnull MRX_DebugResult result)
	{
		m_iDelivered++;
	}
}

//------------------------------------------------------------------------------------------------
class MRX_DebugTestUtils
{
	//------------------------------------------------------------------------------------------------
	//! \param line Values separated by "|" (empty values allowed).
	//! \param[out] outValues The parsed values joined by ",".
	static MRX_EDebugStatus Parse(notnull MRX_DebugAction action, string line, int startIndex, out string outValues, out string outError)
	{
		array<string> words = {};
		if (!line.IsEmpty())
			line.Split("|", words, false);

		array<string> values = {};
		MRX_EDebugStatus status = MRX_DebugArgs.Parse(action, words, startIndex, values, outError);
		outValues = SCR_StringHelper.Join(",", values);
		return status;
	}

	//------------------------------------------------------------------------------------------------
	static string StatusText(MRX_EDebugStatus status)
	{
		return typename.EnumToString(MRX_EDebugStatus, status);
	}
}

//------------------------------------------------------------------------------------------------
//! Defaults for missing and empty values, required values, whole numbers, extra values, skipped leading words.
class MRX_Test_DebugArgs : MRX_TestCase
{
	protected ref MRX_TestDebugArgsAction m_Action = new MRX_TestDebugArgsAction();

	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		Expect("500", 0, MRX_EDebugStatus.OK, "500,default,1", "only the required value");
		Expect("500||", 0, MRX_EDebugStatus.OK, "500,default,1", "empty values (panel fields) take the default");
		Expect("7|usd|3", 0, MRX_EDebugStatus.OK, "7,usd,3", "all values");
		Expect("mrxdbg|test.args|7|usd", 2, MRX_EDebugStatus.OK, "7,usd,1", "leading words skipped");
		Expect("-20", 0, MRX_EDebugStatus.OK, "-20,default,1", "negative whole number");
		Expect("", 0, MRX_EDebugStatus.BAD_ARGS, "", "required value missing");
		Expect("|usd", 0, MRX_EDebugStatus.BAD_ARGS, "", "required value empty");
		Expect("5x", 0, MRX_EDebugStatus.BAD_ARGS, "", "not a whole number");
		Expect("500|usd|two", 0, MRX_EDebugStatus.BAD_ARGS, "", "optional value not a whole number");
		Expect("1234567890", 0, MRX_EDebugStatus.BAD_ARGS, "", "more than 9 digits");
		Expect("-", 0, MRX_EDebugStatus.BAD_ARGS, "", "minus sign alone");
		Expect("1|usd|2|extra", 0, MRX_EDebugStatus.BAD_ARGS, "", "more values than arguments");

		string values, error;
		MRX_DebugTestUtils.Parse(m_Action, "", 0, values, error);
		Check(error.Contains("amount") && error.Contains("Usage: test.args <amount> [currency=default] [count=1]"), "the error names the argument and shows the usage: " + error);
		Finish();
	}

	//------------------------------------------------------------------------------------------------
	protected void Expect(string line, int startIndex, MRX_EDebugStatus expected, string expectedValues, string message)
	{
		string values, error;
		MRX_EDebugStatus status = MRX_DebugTestUtils.Parse(m_Action, line, startIndex, values, error);
		CheckString(MRX_DebugTestUtils.StatusText(status), MRX_DebugTestUtils.StatusText(expected), message + " (status)");
		if (expected == MRX_EDebugStatus.OK)
			CheckString(values, expectedValues, message);
		else
			Check(!error.IsEmpty(), message + ": an error text");
	}
}

//------------------------------------------------------------------------------------------------
//! A second action with a taken ID is refused and the first one stays.
class MRX_Test_DebugRegistry : MRX_TestCase
{
	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		MRX_DebugRegistry registry = new MRX_DebugRegistry();
		MRX_TestDebugArgsAction first = new MRX_TestDebugArgsAction();
		MRX_TestDebugArgsAction second = new MRX_TestDebugArgsAction();
		Check(registry.Register(first), "the first action registers");
		Check(!registry.Register(second), "the same ID again is refused");
		Check(registry.Find("test.args") == first, "the first action stays");

		array<string> ids = {};
		CheckInt(registry.GetIds(ids), 1, "registered IDs");
		Finish();
	}
}

//------------------------------------------------------------------------------------------------
//! Server run: unknown ID, action without a server part, bad values, and a late answer given twice that is
//! delivered once.
class MRX_Test_DebugExecute : MRX_TestCase
{
	protected static const int LATE_MS = 200;

	protected ref MRX_DebugRegistry m_Registry;
	protected ref MRX_TestDebugLateAction m_Late;
	protected ref MRX_TestDebugCountingReply m_LateReply;

	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		if (!MRX_DebugRunner.IsAllowed())
		{
			Skip("not a developer build: every run is REJECTED");
			return;
		}

		m_Registry = new MRX_DebugRegistry();
		m_Late = new MRX_TestDebugLateAction();
		m_Registry.Register(m_Late);
		m_Registry.Register(new MRX_TestDebugArgsAction());

		ExpectNow("test.nothing", "", MRX_EDebugStatus.UNKNOWN_ACTION, "unknown ID");
		ExpectNow("test.args", "500", MRX_EDebugStatus.FAILED, "no server part");
		ExpectNow("test.late", "x", MRX_EDebugStatus.BAD_ARGS, "bad values");
		CheckInt(m_Late.m_iRuns, 0, "the action did not run for bad values");

		m_LateReply = new MRX_TestDebugCountingReply();
		array<string> values = {"2"};
		MRX_DebugRunner.Execute(m_Registry, 0, "test.late", values, m_LateReply);
		CheckInt(m_Late.m_iRuns, 1, "the action runs");
		Check(!m_LateReply.IsDone(), "no answer before the action gives one");
		GetGame().GetCallqueue().CallLater(AnswerLate, LATE_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void AnswerLate()
	{
		m_Late.m_Reply.Done(MRX_DebugResult.Ok("first"));
		m_Late.m_Reply.Done(MRX_DebugResult.Ok("second"));
		CheckInt(m_LateReply.m_iDelivered, 1, "delivered once");
		if (m_LateReply.GetResult())
			CheckString(m_LateReply.GetResult().m_sText, "first", "the first answer counts");

		Finish();
	}

	//------------------------------------------------------------------------------------------------
	protected void ExpectNow(string actionId, string line, MRX_EDebugStatus expected, string message)
	{
		array<string> values = {};
		if (!line.IsEmpty())
			line.Split("|", values, false);

		MRX_TestDebugCountingReply reply = new MRX_TestDebugCountingReply();
		MRX_DebugRunner.Execute(m_Registry, 0, actionId, values, reply);
		CheckInt(reply.m_iDelivered, 1, message + ": answered at once");
		if (reply.GetResult())
			CheckString(MRX_DebugTestUtils.StatusText(reply.GetResult().m_eStatus), MRX_DebugTestUtils.StatusText(expected), message);
	}
}
#endif
