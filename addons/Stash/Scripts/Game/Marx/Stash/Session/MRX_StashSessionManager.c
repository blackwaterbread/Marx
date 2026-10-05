//! Server: open stash containers, one per player (API v0). Created by the Marx system; use MRX_StashSessions.Get().
class MRX_StashSessionManager : Managed
{
	static const ResourceName CONTAINER_PREFAB = "{82A52DBC72E19BE8}Prefabs/Marx/Stash/MRX_StashContainer.et";
	protected static const int CHECK_INTERVAL_MS = 1000;

	protected ref map<int, ref MRX_StashSession> m_mSessions = new map<int, ref MRX_StashSession>();
	//! Closed sessions, released on a later frame (never inside their own call stack).
	protected ref array<ref MRX_StashSession> m_aClosed = {};

	//------------------------------------------------------------------------------------------------
	void MRX_StashSessionManager()
	{
		MRX_StashStorageComponent.GetOnItemMoved().Insert(OnContainerItemMoved);
		GetGame().GetCallqueue().CallLater(CheckSessions, CHECK_INTERVAL_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	void ~MRX_StashSessionManager()
	{
		GetGame().GetCallqueue().Remove(CheckSessions);
		MRX_StashStorageComponent.GetOnItemMoved().Remove(OnContainerItemMoved);
	}

	//------------------------------------------------------------------------------------------------
	//! Opens the player's stash at the stash point (the caller checked the distance).
	//! A stash that is still open or closing answers BUSY (and an open one starts closing).
	MRX_EStashStatus Open(int playerId, notnull IEntity stashPoint)
	{
		string ownerId = MRX_Marx.GetOwnerId(playerId);
		if (ownerId.IsEmpty())
			return MRX_EStashStatus.OWNER_NOT_READY;

		IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!IsAlive(character))
			return MRX_EStashStatus.NOT_IN_INVENTORY;

		MRX_StashSession current = m_mSessions.Get(playerId);
		if (current)
		{
			current.Close("opened again");
			return MRX_EStashStatus.BUSY;
		}

		MRX_StashSession session = new MRX_StashSession(this, playerId, ownerId, character, stashPoint);
		m_mSessions.Set(playerId, session);
		if (!session.Start(CONTAINER_PREFAB))
		{
			m_mSessions.Remove(playerId);
			return MRX_EStashStatus.SPAWN_FAILED;
		}

		return MRX_EStashStatus.OK;
	}

	//------------------------------------------------------------------------------------------------
	void Close(int playerId, string reason)
	{
		MRX_StashSession session = m_mSessions.Get(playerId);
		if (session)
			session.Close(reason);
	}

	//------------------------------------------------------------------------------------------------
	//! \return Open session of the player, or null.
	MRX_StashSession Find(int playerId)
	{
		return m_mSessions.Get(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Internal: a character's inventory changed (e.g. an item taken out of a bag inside the container).
	void OnCharacterInventoryChanged(IEntity character)
	{
		foreach (int playerId, MRX_StashSession session : m_mSessions)
		{
			if (session.GetCharacter() == character)
				session.MarkDirty();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, called by MRX_StashSession: the container is filled; the client may open it.
	void OnSessionReady(notnull MRX_StashSession session)
	{
		SCR_PlayerController controller = GetController(session.GetPlayerId());
		if (controller)
			controller.MRX_SendStashContainerOpen(session.GetContainer());
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, called by MRX_StashSession when it starts closing.
	void OnSessionClosing(notnull MRX_StashSession session)
	{
		SCR_PlayerController controller = GetController(session.GetPlayerId());
		if (controller)
			controller.MRX_SendStashContainerClose();
	}

	//------------------------------------------------------------------------------------------------
	//! Internal, called by MRX_StashSession once the container is gone.
	void OnSessionClosed(notnull MRX_StashSession session)
	{
		if (m_mSessions.Get(session.GetPlayerId()) == session)
			m_mSessions.Remove(session.GetPlayerId());

		if (m_aClosed.IsEmpty())
			GetGame().GetCallqueue().CallLater(ReleaseClosed);

		m_aClosed.Insert(session);
	}

	//------------------------------------------------------------------------------------------------
	//! Internal: tells the player about a refused or failed move.
	void ReportResult(notnull MRX_StashSession session, MRX_EStashStatus status)
	{
		SCR_PlayerController controller = GetController(session.GetPlayerId());
		if (controller)
			controller.MRX_SendStashResult(status, string.Empty);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnContainerItemMoved(MRX_StashStorageComponent storage, IEntity item, bool added)
	{
		foreach (int playerId, MRX_StashSession session : m_mSessions)
		{
			if (session.GetStorage() == storage)
				session.MarkDirty();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Closes sessions whose player left, died or walked away from the stash point.
	protected void CheckSessions()
	{
		array<MRX_StashSession> toClose = {};
		array<string> reasons = {};
		foreach (int playerId, MRX_StashSession session : m_mSessions)
		{
			if (session.IsClosing())
				continue;

			IEntity character = session.GetCharacter();
			IEntity stashPoint = session.GetStashPoint();
			MRX_StashPointComponent point;
			if (stashPoint)
				point = MRX_StashPointComponent.Cast(stashPoint.FindComponent(MRX_StashPointComponent));

			string reason;
			if (GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId) != character)
				reason = "player no longer controls the character";
			else if (!IsAlive(character))
				reason = "character dead";
			else if (!point)
				reason = "stash point gone";
			else if (!point.IsInRange(character))
				reason = "out of range";

			if (reason.IsEmpty())
				continue;

			toClose.Insert(session);
			reasons.Insert(reason);
		}

		foreach (int i, MRX_StashSession closing : toClose)
		{
			closing.Close(reasons[i]);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void ReleaseClosed()
	{
		m_aClosed.Clear();
	}

	//------------------------------------------------------------------------------------------------
	protected static SCR_PlayerController GetController(int playerId)
	{
		return SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsAlive(IEntity character)
	{
		ChimeraCharacter chimera = ChimeraCharacter.Cast(character);
		if (!chimera || !chimera.GetCharacterController())
			return false;

		return chimera.GetCharacterController().GetLifeState() == ECharacterLifeState.ALIVE;
	}
}
