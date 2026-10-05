//! What happens to deployed assets carried by a player who dies.
enum MRX_EDeathAction
{
	//! The items stay on the body. The asset is lost when its item is destroyed or removed (e.g. by garbage collection).
	KEEP,
	//! Lost at once. The items stay in the world as ordinary loot.
	LOSE,
	//! Taken off the body and put back into the owner's stash.
	RETURN
}

//! What happens to assets that were DEPLOYED in an earlier server session (restart or crash).
enum MRX_ERecoveryAction
{
	//! Back to the stash with their last stored state (the state at withdraw time).
	RESTORE,
	LOSE
}

//! Loss policy (API v0). Set the actions in the Marx settings, or subclass it and pass it to MRX_StashService.SetLossPolicy().
//! Independent of the policy: an item that disappears while its owner is online is LOST (destroyed, removed, used up),
//! and one that disappears while the owner is offline (e.g. deleted with the character on disconnect) is restored.
[BaseContainerProps()]
class MRX_LossPolicy
{
	[Attribute("0", UIWidgets.ComboBox, "Deployed assets carried by a player who dies", enums: ParamEnumArray.FromEnum(MRX_EDeathAction))]
	MRX_EDeathAction m_eOnDeath;

	[Attribute("0", UIWidgets.ComboBox, "Assets deployed in an earlier server session", enums: ParamEnumArray.FromEnum(MRX_ERecoveryAction))]
	MRX_ERecoveryAction m_eAfterRestart;

	//------------------------------------------------------------------------------------------------
	//! Policy used when nothing is configured. Attribute defaults only apply to instances loaded from a config.
	static MRX_LossPolicy Create(MRX_EDeathAction onDeath = MRX_EDeathAction.KEEP, MRX_ERecoveryAction afterRestart = MRX_ERecoveryAction.RESTORE)
	{
		MRX_LossPolicy policy = new MRX_LossPolicy();
		policy.m_eOnDeath = onDeath;
		policy.m_eAfterRestart = afterRestart;
		return policy;
	}

	//------------------------------------------------------------------------------------------------
	//! Override for per-asset rules.
	MRX_EDeathAction GetDeathAction(notnull MRX_AssetBinding binding)
	{
		return m_eOnDeath;
	}

	//------------------------------------------------------------------------------------------------
	//! Override for per-asset rules.
	MRX_ERecoveryAction GetRecoveryAction(notnull MRX_AssetRecord asset)
	{
		return m_eAfterRestart;
	}
}
