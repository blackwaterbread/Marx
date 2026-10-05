//! Life cycle of an owned asset (API v0).
//! STASHED <-> DEPLOYED, DEPLOYED -> LOST | CONSUMED. LOST and CONSUMED are final: the record leaves the stash.
enum MRX_EAssetState
{
	//! Kept in the owner's stash, not in the world.
	STASHED,
	//! Spawned in the world and bound to its entity.
	DEPLOYED,
	//! Destroyed, or lost by a loss policy.
	LOST,
	//! Used up, reported by the consuming mod.
	CONSUMED
}

//! Outcome of a stash operation (API v0).
enum MRX_EStashStatus
{
	OK,
	//! The idempotency key was already committed. Nothing changed.
	DUPLICATE,
	//! Owner ID is empty (e.g. the player's identity is not resolved yet).
	OWNER_NOT_READY,
	//! Missing source or idempotency key.
	INVALID_CONTEXT,
	UNKNOWN_ASSET,
	//! The asset is not in the state the operation needs, or the transition is not allowed.
	INVALID_STATE,
	//! A new asset uses an ID that already exists.
	ASSET_EXISTS,
	//! Missing or unloadable prefab.
	INVALID_PREFAB,
	//! The stash holds the maximum number of assets.
	STASH_FULL,
	//! Refused by a registered MRX_StashValidator.
	REJECTED,
	//! The item is not in the player's inventory, or is an entity Marx cannot store.
	NOT_IN_INVENTORY,
	//! The item is bound to an asset of another owner.
	NOT_OWNED,
	//! The item could not be read into a snapshot.
	CAPTURE_FAILED,
	//! The item could not be spawned or delivered.
	SPAWN_FAILED,
	//! The player's inventory has no room for the item.
	NO_SPACE,
	STORAGE_ERROR,
	//! Another operation of the same asset or player is running.
	BUSY
}

//------------------------------------------------------------------------------------------------
//! Allowed state changes. No engine dependencies.
class MRX_AssetStates
{
	//------------------------------------------------------------------------------------------------
	static bool IsFinal(MRX_EAssetState state)
	{
		return state == MRX_EAssetState.LOST || state == MRX_EAssetState.CONSUMED;
	}

	//------------------------------------------------------------------------------------------------
	//! Same-state updates (e.g. a new snapshot of a deployed asset) are allowed for non-final states.
	static bool CanTransition(MRX_EAssetState from, MRX_EAssetState to)
	{
		if (IsFinal(from))
			return false;

		if (from == to)
			return true;

		if (from == MRX_EAssetState.STASHED)
			return to == MRX_EAssetState.DEPLOYED;

		// DEPLOYED -> STASHED, LOST or CONSUMED
		return true;
	}
}
