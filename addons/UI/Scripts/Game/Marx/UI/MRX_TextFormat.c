//! Text formatting shared by the Marx UI (API v0).
class MRX_TextFormat
{
	//! Display formats by currency ID, set by the consumer on each machine.
	protected static ref map<string, string> s_mCurrencyFormats;

	//------------------------------------------------------------------------------------------------
	//! Whole number with thousands separators, e.g. 1234567 -> "1,234,567", -2500 -> "-2,500".
	static string Amount(int amount)
	{
		string digits = amount.ToString();
		string sign;
		if (amount < 0)
		{
			sign = "-";
			digits = digits.Substring(1, digits.Length() - 1);
		}

		string grouped;
		int length = digits.Length();
		for (int i = 0; i < length; i++)
		{
			if (i > 0 && (length - i) % 3 == 0)
				grouped += ",";

			grouped += digits.Get(i);
		}

		return sign + grouped;
	}

	//------------------------------------------------------------------------------------------------
	//! Amount in a currency: with a display format set for it, e.g. "$12,500"; otherwise amount and currency ID,
	//! e.g. "12,500 cash". A minus sign goes in front of the format: "-$2,500".
	static string Money(int amount, string currency)
	{
		string format;
		if (!s_mCurrencyFormats || !s_mCurrencyFormats.Find(currency, format))
			return Amount(amount) + " " + currency;

		string text = Amount(amount);
		if (amount < 0)
			return "-" + string.Format(format, text.Substring(1, text.Length() - 1));

		return string.Format(format, text);
	}

	//------------------------------------------------------------------------------------------------
	//! At most four characters for narrow places (rounded down): 950 -> "950", 1500 -> "1.5k", 14500 -> "14k",
	//! 2500000 -> "2.5m", 2147483647 -> "2.1b". A minus sign comes in front.
	static string AmountCompact(int amount)
	{
		if (amount < 0)
		{
			// int.MIN has no positive counterpart; int.MAX shows the same.
			if (amount == int.MIN)
				amount = int.MIN + 1;

			return "-" + AmountCompact(-amount);
		}

		if (amount < 1000)
			return amount.ToString();

		if (amount < 1000000)
			return Scaled(amount, 1000, "k");

		if (amount < 1000000000)
			return Scaled(amount, 1000000, "m");

		return Scaled(amount, 1000000000, "b");
	}

	//------------------------------------------------------------------------------------------------
	//! Like Money() with AmountCompact(), e.g. "$14k". Without a display format only the amount, e.g. "14k": the currency
	//! ID does not fit narrow places.
	static string MoneyCompact(int amount, string currency)
	{
		string format;
		if (!s_mCurrencyFormats || !s_mCurrencyFormats.Find(currency, format))
			return AmountCompact(amount);

		string text = AmountCompact(amount);
		if (amount < 0)
			return "-" + string.Format(format, text.Substring(1, text.Length() - 1));

		return string.Format(format, text);
	}

	//------------------------------------------------------------------------------------------------
	//! One decimal below 10 units, none above; ".0" is dropped.
	protected static string Scaled(int amount, int unit, string suffix)
	{
		int whole = amount / unit;
		if (whole >= 10)
			return whole.ToString() + suffix;

		int tenth = (amount % unit) / (unit / 10);
		if (tenth == 0)
			return whole.ToString() + suffix;

		return whole.ToString() + "." + tenth.ToString() + suffix;
	}

	//------------------------------------------------------------------------------------------------
	//! Sets how amounts of a currency are shown on this machine. Display only: storage, ledger and API keep the
	//! currency ID. Call it on every machine (server and clients), e.g. from the game mode's EOnInit.
	//! \param format %1 is the amount with thousands separators, e.g. "$%1" or "%1 cr". Empty restores the default.
	static void SetCurrencyFormat(string currency, string format)
	{
		if (!s_mCurrencyFormats)
			s_mCurrencyFormats = new map<string, string>();

		if (format.IsEmpty())
			s_mCurrencyFormats.Remove(currency);
		else
			s_mCurrencyFormats.Set(currency, format);
	}
}
