//! Creates the default wallet HUD for the local player (see MRX_WalletHud.SetEnabled).
modded class SCR_PlayerController
{
	protected ref MRX_WalletHud m_MRX_WalletHud;

	//------------------------------------------------------------------------------------------------
	override void OnControlledEntityChanged(IEntity from, IEntity to)
	{
		super.OnControlledEntityChanged(from, to);

		// Local player only; the HUD root exists once the player controls an entity.
		if (to && !m_MRX_WalletHud && GetGame().GetPlayerController() == this)
			m_MRX_WalletHud = MRX_WalletHud.Create(MRX_GetWallet());
	}
}
