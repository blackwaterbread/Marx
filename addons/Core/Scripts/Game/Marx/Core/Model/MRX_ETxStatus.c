//! Outcome of an economy operation (API v0).
enum MRX_ETxStatus
{
	OK,
	//! The idempotency key was already committed. The result carries the original transaction ID (and its entries while still retained).
	DUPLICATE,
	INSUFFICIENT_FUNDS,
	//! Amount is zero, negative or out of range.
	INVALID_AMOUNT,
	UNKNOWN_CURRENCY,
	//! Resulting balance would exceed the currency limits.
	LIMIT_EXCEEDED,
	//! Owner ID is empty (e.g. the player's identity is not resolved yet).
	OWNER_NOT_READY,
	//! Owner combination is not allowed (e.g. transfer to self).
	INVALID_OWNER,
	//! Missing source or idempotency key.
	INVALID_CONTEXT,
	//! Refused by a registered MRX_TxValidator.
	REJECTED,
	STORAGE_ERROR,
	BUSY
}
