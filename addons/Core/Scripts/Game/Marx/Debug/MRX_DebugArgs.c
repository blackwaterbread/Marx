//! Argument values of debug actions: positional, missing or empty values take the argument's default.
class MRX_DebugArgs
{
	//! Up to 999 999 999, so a value always fits an int.
	protected static const int MAX_INT_DIGITS = 9;

	//------------------------------------------------------------------------------------------------
	//! \param words Values in argument order, from startIndex on.
	//! \param[out] outValues One value per argument when OK.
	//! \param[out] outError Why the values were refused, with the usage; empty when OK.
	//! \return OK, or BAD_ARGS for a missing required value, a value that is not a whole number where one is needed, or
	//! more values than arguments.
	static MRX_EDebugStatus Parse(notnull MRX_DebugAction action, notnull array<string> words, int startIndex, notnull array<string> outValues, out string outError)
	{
		outValues.Clear();
		outError = string.Empty;
		array<ref MRX_DebugArg> args = {};
		action.GetArgs(args);
		if (words.Count() - startIndex > args.Count())
		{
			outError = "Too many values. " + GetUsage(action);
			return MRX_EDebugStatus.BAD_ARGS;
		}

		foreach (int i, MRX_DebugArg arg : args)
		{
			string value;
			if (startIndex + i < words.Count())
				value = words[startIndex + i].Trim();

			if (value.IsEmpty())
				value = arg.m_sDefault;

			if (value.IsEmpty())
			{
				outError = string.Format("Missing %1. %2", arg.m_sName, GetUsage(action));
				return MRX_EDebugStatus.BAD_ARGS;
			}

			if (arg.m_bInt && !IsInt(value))
			{
				outError = string.Format("%1 must be a whole number, not '%2'. %3", arg.m_sName, value, GetUsage(action));
				return MRX_EDebugStatus.BAD_ARGS;
			}

			outValues.Insert(value);
		}

		return MRX_EDebugStatus.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! E.g. "Usage: wallet.give <amount> [currency=default] [player=me]".
	static string GetUsage(notnull MRX_DebugAction action)
	{
		string usage = "Usage: " + action.GetId();
		array<ref MRX_DebugArg> args = {};
		action.GetArgs(args);
		foreach (MRX_DebugArg arg : args)
		{
			if (arg.IsRequired())
				usage += string.Format(" <%1>", arg.m_sName);
			else
				usage += string.Format(" [%1=%2]", arg.m_sName, arg.m_sDefault);
		}

		return usage;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True for a whole number of up to 9 digits, optionally negative.
	static bool IsInt(string text)
	{
		int start;
		if (text.StartsWith("-"))
			start = 1;

		int length = text.Length();
		if (length <= start || length - start > MAX_INT_DIGITS)
			return false;

		for (int i = start; i < length; i++)
		{
			if (!text.IsDigitAt(i))
				return false;
		}

		return true;
	}
}
