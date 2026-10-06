//! Text formatting shared by the Marx UI (API v0).
class MRX_TextFormat
{
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
	//! Amount and currency ID, e.g. "12,500 cash".
	static string Money(int amount, string currency)
	{
		return Amount(amount) + " " + currency;
	}
}
