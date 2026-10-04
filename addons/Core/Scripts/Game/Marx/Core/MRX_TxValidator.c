//! Extension point (API v0): refuse transactions before they are queued.
//! Register with MRX_EconomyService.AddValidator(). Runs on the server.
class MRX_TxValidator : Managed
{
	//------------------------------------------------------------------------------------------------
	//! \return False to refuse the transaction with REJECTED.
	bool Validate(MRX_TxRequest request)
	{
		return true;
	}
}
