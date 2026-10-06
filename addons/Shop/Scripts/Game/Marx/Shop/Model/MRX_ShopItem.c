//! One catalog entry (API v0).
[BaseContainerProps(), BaseContainerCustomTitleField("m_sId")]
class MRX_ShopItem
{
	[Attribute(desc: "Item ID, unique within the catalog. Sent in purchase requests and written to the ledger reason.")]
	string m_sId;

	[Attribute(params: "et", uiwidget: UIWidgets.ResourcePickerThumbnail)]
	ResourceName m_sPrefab;

	[Attribute(desc: "Display name. Empty uses the prefab's name.")]
	LocalizedString m_sName;

	[Attribute(desc: "Category shown in the shop UI")]
	string m_sCategory;

	[Attribute(desc: "Shown below the name in the shop window")]
	LocalizedString m_sDescription;

	[Attribute(desc: "Product that is not an inventory item, e.g. a stash upgrade: the shop has it delivered instead of giving the prefab, which is then only shown (optional). Products are not bought back.")]
	ref MRX_ShopProduct m_Product;

	[Attribute("cash")]
	string m_sCurrency;

	[Attribute("0", desc: "Purchase price. 0 or less: not for sale.")]
	int m_iPrice;

	[Attribute("-1", desc: "Price the shop pays when buying the item back. -1: the shop's sell percentage of the price. 0: not bought back.")]
	int m_iSellPrice;

	[Attribute("-1", desc: "Reserved for stock limits. -1: unlimited (stock is not enforced yet).")]
	int m_iStock;

	//------------------------------------------------------------------------------------------------
	static MRX_ShopItem Create(string id, ResourceName prefab, int price, string currency = "cash", int sellPrice = -1)
	{
		MRX_ShopItem item = new MRX_ShopItem();
		item.m_sId = id;
		item.m_sPrefab = prefab;
		item.m_iPrice = price;
		item.m_sCurrency = currency;
		item.m_iSellPrice = sellPrice;
		item.m_iStock = -1;
		return item;
	}
}
