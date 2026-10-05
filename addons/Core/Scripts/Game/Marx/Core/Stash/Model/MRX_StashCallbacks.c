// Stash callbacks (API v0), same pattern as the economy callbacks in MRX_Callbacks.c.

void MRX_StashDelegate(MRX_EStashStatus status, MRX_StashRecord record);
typedef func MRX_StashDelegate;

void MRX_StashResultDelegate(MRX_StashResult result);
typedef func MRX_StashResultDelegate;

void MRX_AssetStateChangedDelegate(MRX_AssetRecord asset, MRX_EAssetState oldState);
typedef func MRX_AssetStateChangedDelegate;

//------------------------------------------------------------------------------------------------
//! Receives a detached copy of a stash.
class MRX_StashCallback : Managed
{
	protected ref ScriptInvokerBase<MRX_StashDelegate> m_OnResult;

	//------------------------------------------------------------------------------------------------
	ScriptInvokerBase<MRX_StashDelegate> GetOnResult()
	{
		if (!m_OnResult)
			m_OnResult = new ScriptInvokerBase<MRX_StashDelegate>();

		return m_OnResult;
	}

	//------------------------------------------------------------------------------------------------
	//! Override to handle the result without subscribing.
	void OnResult(MRX_EStashStatus status, MRX_StashRecord record)
	{
		if (m_OnResult)
			m_OnResult.Invoke(status, record);
	}
}

//------------------------------------------------------------------------------------------------
class MRX_StashResultCallback : Managed
{
	protected ref ScriptInvokerBase<MRX_StashResultDelegate> m_OnResult;

	//------------------------------------------------------------------------------------------------
	ScriptInvokerBase<MRX_StashResultDelegate> GetOnResult()
	{
		if (!m_OnResult)
			m_OnResult = new ScriptInvokerBase<MRX_StashResultDelegate>();

		return m_OnResult;
	}

	//------------------------------------------------------------------------------------------------
	//! Override to handle the result without subscribing.
	void OnResult(MRX_StashResult result)
	{
		if (m_OnResult)
			m_OnResult.Invoke(result);
	}
}

//------------------------------------------------------------------------------------------------
class MRX_StashDelivery : MRX_DeferredCall
{
	protected ref MRX_StashCallback m_Callback;
	protected MRX_EStashStatus m_eStatus;
	protected ref MRX_StashRecord m_Record;

	//------------------------------------------------------------------------------------------------
	void MRX_StashDelivery(MRX_StashCallback callback, MRX_EStashStatus status, MRX_StashRecord record)
	{
		m_Callback = callback;
		m_eStatus = status;
		m_Record = record;
	}

	//------------------------------------------------------------------------------------------------
	override void Run()
	{
		m_Callback.OnResult(m_eStatus, m_Record);
	}

	//------------------------------------------------------------------------------------------------
	static void Post(notnull MRX_CallQueue queue, MRX_StashCallback callback, MRX_EStashStatus status, MRX_StashRecord record)
	{
		if (callback)
			queue.Post(new MRX_StashDelivery(callback, status, record));
	}
}

//------------------------------------------------------------------------------------------------
class MRX_StashResultDelivery : MRX_DeferredCall
{
	protected ref MRX_StashResultCallback m_Callback;
	protected ref MRX_StashResult m_Result;

	//------------------------------------------------------------------------------------------------
	void MRX_StashResultDelivery(MRX_StashResultCallback callback, MRX_StashResult result)
	{
		m_Callback = callback;
		m_Result = result;
	}

	//------------------------------------------------------------------------------------------------
	override void Run()
	{
		m_Callback.OnResult(m_Result);
	}

	//------------------------------------------------------------------------------------------------
	static void Post(notnull MRX_CallQueue queue, MRX_StashResultCallback callback, MRX_StashResult result)
	{
		if (callback)
			queue.Post(new MRX_StashResultDelivery(callback, result));
	}
}
