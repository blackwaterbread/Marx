//! Support station actions of a Marx arsenal (e.g. the arsenal box's resupply actions) are hidden: they would hand out
//! listed magazines and medical items without payment. The server checks this before performing them too.
modded class SCR_BaseUseSupportStationAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (MRX_ArsenalShopComponent.Find(GetOwner()))
			return false;

		return super.CanBeShownScript(user);
	}
}
