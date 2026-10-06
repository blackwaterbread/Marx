//! Example consumer: shows the "cash" currency as dollars ("$12,500") in the Marx UI. Display only: storage, the ledger
//! and the API keep the currency ID. Runs on every machine, as MRX_TextFormat.SetCurrencyFormat() requires.
modded class SCR_BaseGameMode
{
	//! Identifies the currency of the Marx settings that this example formats.
	protected static const string MRX_EXAMPLE_CURRENCY = "cash";
	protected static const string MRX_EXAMPLE_CURRENCY_FORMAT = "$%1";

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);
		MRX_TextFormat.SetCurrencyFormat(MRX_EXAMPLE_CURRENCY, MRX_EXAMPLE_CURRENCY_FORMAT);
	}
}
