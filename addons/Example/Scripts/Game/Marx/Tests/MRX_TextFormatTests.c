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
		CheckString(MRX_TextFormat.Money(12500, "cash"), "12,500 cash", "money");
		Finish();
	}
}
#endif
