#ifdef WORKBENCH
// Runtime spike: per-player wallet as a non-singleton scripted state stored in a Marx collection.
// Needs a Workbench-authored persistence config: collection "MarxSpike" (on a GamemodeStorage)
// and a StatePersistenceConfig using MRX_SpikeWalletSerializer. Temporary - remove before release.

//------------------------------------------------------------------------------------------------
class MRX_SpikeWallet : Managed
{
	string m_sOwner;
	int m_iBalance;
	int m_iLoadCount;
}

//------------------------------------------------------------------------------------------------
class MRX_SpikeWalletSerializer : ScriptedStateSerializer
{
	//------------------------------------------------------------------------------------------------
	override static typename GetTargetType()
	{
		return MRX_SpikeWallet;
	}

	//------------------------------------------------------------------------------------------------
	override protected ESerializeResult Serialize(notnull Managed instance, notnull SaveContext context)
	{
		MRX_SpikeWallet wallet = MRX_SpikeWallet.Cast(instance);
		context.WriteValue("version", 1);
		context.WriteValue("owner", wallet.m_sOwner);
		context.WriteValue("balance", wallet.m_iBalance);
		context.WriteValue("loadCount", wallet.m_iLoadCount);
		MRX_Spike.Log(string.Format("wallet: Serialize owner=%1 balance=%2", wallet.m_sOwner, wallet.m_iBalance));
		return ESerializeResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	override protected bool Deserialize(notnull Managed instance, notnull LoadContext context)
	{
		MRX_SpikeWallet wallet = MRX_SpikeWallet.Cast(instance);
		int version;
		context.ReadValue("version", version);
		context.ReadValue("owner", wallet.m_sOwner);
		context.ReadValue("balance", wallet.m_iBalance);
		context.ReadValue("loadCount", wallet.m_iLoadCount);
		MRX_Spike.Log(string.Format("wallet: Deserialize version=%1 owner=%2 balance=%3", version, wallet.m_sOwner, wallet.m_iBalance));
		return true;
	}
}

//------------------------------------------------------------------------------------------------
class MRX_SpikeWalletProbe
{
	static const string COLLECTION_NAME = "MarxSpike";

	protected UUID m_sWalletId;
	protected string m_sIdentity;
	protected ref MRX_SpikeWallet m_Wallet;
	protected ref PersistenceResultCallback m_LoadCallback;
	protected ref PersistenceStatusCallback m_CommitCallback;
	protected int m_iStart;

	//------------------------------------------------------------------------------------------------
	//! Vanilla SCR_SpawnLogic pattern: create the instance, assign its id, then RequestLoad applies saved data onto it.
	//! (Run 3 showed RequestSpawn returns an untracked instance whose id cannot be re-assigned -> duplicate record.)
	void Start(UUID identity)
	{
		PersistenceSystem sys = PersistenceSystem.GetInstance();
		if (!sys)
		{
			MRX_Spike.Log("wallet: no PersistenceSystem");
			return;
		}

		if (!sys.FindCollection(COLLECTION_NAME))
		{
			MRX_Spike.Log("wallet: collection '" + COLLECTION_NAME + "' not found - Marx spike persistence config not active");
			return;
		}

		m_sIdentity = identity;
		m_sWalletId = PersistenceIdUtils.FromString("marx:wallet:" + identity);
		m_Wallet = new MRX_SpikeWallet();
		bool idSet = sys.SetId(m_Wallet, m_sWalletId, true);
		MRX_Spike.Log(string.Format("wallet: SetId=%1 tracked=%2 id=%3 config=%4",
			idSet, sys.IsTracked(m_Wallet), sys.GetId(m_Wallet), sys.GetConfig(m_Wallet) != null));

		PersistenceLoadRequest request = new PersistenceLoadRequest();
		request.Instances = {m_Wallet};
		m_LoadCallback = new PersistenceResultCallback(OnWalletLoaded);
		m_iStart = System.GetTickCount();
		sys.RequestLoad(request, m_LoadCallback);
		MRX_Spike.Log(string.Format("wallet: RequestLoad id=%1 identity=%2", m_sWalletId, identity));
	}

	//------------------------------------------------------------------------------------------------
	protected void OnWalletLoaded(EPersistenceStatusCode statusCode, Managed result, bool isLast, Managed context)
	{
		PersistenceSystem sys = PersistenceSystem.GetInstance();
		MRX_Spike.Log(string.Format("wallet: RequestLoad result=%1 ms=%2 isLast=%3 sameInstance=%4",
			typename.EnumToString(EPersistenceStatusCode, statusCode), System.GetTickCount() - m_iStart, isLast, result == m_Wallet));

		if (statusCode == EPersistenceStatusCode.OK)
		{
			m_Wallet.m_iLoadCount++;
			m_Wallet.m_iBalance += 1;
			MRX_Spike.Log(string.Format("wallet: LOADED owner=%1 balance=%2 loadCount=%3", m_Wallet.m_sOwner, m_Wallet.m_iBalance, m_Wallet.m_iLoadCount));
		}
		else
		{
			m_Wallet.m_sOwner = m_sIdentity;
			m_Wallet.m_iBalance = 100;
			MRX_Spike.Log("wallet: CREATED balance=100");
		}

		MRX_Spike.Log(string.Format("wallet: before save tracked=%1 id=%2", sys.IsTracked(m_Wallet), sys.GetId(m_Wallet)));
		MRX_Spike.Log("wallet: Save=" + sys.Save(m_Wallet));
		m_CommitCallback = new PersistenceStatusCallback(OnCommitted);
		m_iStart = System.GetTickCount();
		sys.CommitStorage(GamemodeStorage, m_CommitCallback);
		MRX_Spike.Log("wallet: CommitStorage(GamemodeStorage) requested");
	}

	//------------------------------------------------------------------------------------------------
	protected void OnCommitted(EPersistenceStatusCode statusCode, Managed context)
	{
		MRX_Spike.Log(string.Format("wallet: CommitStorage(GamemodeStorage) result=%1 ms=%2",
			typename.EnumToString(EPersistenceStatusCode, statusCode), System.GetTickCount() - m_iStart));
	}
}
#endif
