[ComponentEditorProps(category: "Marx", description: "Makes the entity an access point to the player's stash. The entity also needs an enabled RplComponent.")]
class MRX_StashPointComponentClass : ScriptComponentClass
{
}

//! Stash access point (API v0): players near it can list, deposit and withdraw their stashed assets.
//! The server checks the distance on every request (MRX_RequestStash* on the player controller).
class MRX_StashPointComponent : ScriptComponent
{
	[Attribute("5", UIWidgets.Slider, "Maximum distance in meters between the player and the stash point", "1 50 0.5")]
	protected float m_fMaxDistance;

	//------------------------------------------------------------------------------------------------
	float GetMaxDistance()
	{
		return m_fMaxDistance;
	}

	//------------------------------------------------------------------------------------------------
	//! True when the entity is within reach of the stash point.
	bool IsInRange(IEntity entity)
	{
		if (!entity)
			return false;

		return vector.DistanceSq(entity.GetOrigin(), GetOwner().GetOrigin()) <= m_fMaxDistance * m_fMaxDistance;
	}
}
