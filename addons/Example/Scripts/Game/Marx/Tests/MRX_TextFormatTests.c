#ifdef WORKBENCH
// Synchronous tests of MRX_TextFormat.

//------------------------------------------------------------------------------------------------
class MRX_TextFormatTests
{
	//------------------------------------------------------------------------------------------------
	static void Register(notnull MRX_TestRunner runner)
	{
		runner.Add(new MRX_Test_TextFormatAmount());
	}
}

//------------------------------------------------------------------------------------------------
class MRX_Test_TextFormatAmount : MRX_TestCase
{
	//------------------------------------------------------------------------------------------------
	override protected void Run()
	{
		CheckString(MRX_TextFormat.Amount(0), "0", "zero");
		CheckString(MRX_TextFormat.Amount(999), "999", "no separator below a thousand");
		CheckString(MRX_TextFormat.Amount(1000), "1,000", "thousand");
		CheckString(MRX_TextFormat.Amount(25000), "25,000", "tens of thousands");
		CheckString(MRX_TextFormat.Amount(1234567), "1,234,567", "millions");
		CheckString(MRX_TextFormat.Amount(-2500), "-2,500", "negative");
		CheckString(MRX_TextFormat.Amount(-999), "-999", "negative below a thousand");
		CheckString(MRX_TextFormat.Amount(int.MIN), "-2,147,483,648", "int.MIN");
		CheckString(MRX_TextFormat.Money(12500, "test_plain"), "12,500 test_plain", "money");

		// A display format for a currency nobody else uses, removed again at the end.
		MRX_TextFormat.SetCurrencyFormat("test_usd", "$%1");
		CheckString(MRX_TextFormat.Money(12500, "test_usd"), "$12,500", "currency with a display format");
		CheckString(MRX_TextFormat.Money(-2500, "test_usd"), "-$2,500", "negative amount with a display format");
		CheckString(MRX_TextFormat.Money(12500, "test_plain"), "12,500 test_plain", "other currencies keep the default");
		CheckString(MRX_TextFormat.MoneyCompact(14500, "test_usd"), "$14k", "compact money with a display format");
		CheckString(MRX_TextFormat.MoneyCompact(-1500, "test_usd"), "-$1.5k", "negative compact money");
		MRX_TextFormat.SetCurrencyFormat("test_usd", string.Empty);
		CheckString(MRX_TextFormat.Money(12500, "test_usd"), "12,500 test_usd", "empty format restores the default");

		CheckString(MRX_TextFormat.AmountCompact(950), "950", "compact below a thousand");
		CheckString(MRX_TextFormat.AmountCompact(1000), "1k", "compact thousand without decimal");
		CheckString(MRX_TextFormat.AmountCompact(1550), "1.5k", "compact rounds down to a tenth");
		CheckString(MRX_TextFormat.AmountCompact(9999), "9.9k", "compact keeps four characters");
		CheckString(MRX_TextFormat.AmountCompact(14500), "14k", "compact tens of thousands");
		CheckString(MRX_TextFormat.AmountCompact(999999), "999k", "compact below a million");
		CheckString(MRX_TextFormat.AmountCompact(2500000), "2.5m", "compact millions");
		CheckString(MRX_TextFormat.AmountCompact(int.MAX), "2.1b", "compact int.MAX");
		CheckString(MRX_TextFormat.AmountCompact(-1500), "-1.5k", "compact negative");
		CheckString(MRX_TextFormat.AmountCompact(int.MIN), "-2.1b", "compact int.MIN");
		CheckString(MRX_TextFormat.MoneyCompact(1500, "test_plain"), "1.5k", "compact money without a display format is the amount only");
		Finish();
	}
}
#endif
