// Development tool: creates the sample assets (Marx_Shop: shop catalog, shop table and arsenal box, Marx_Stash: stash wardrobe).
// Needs an open world: prefabs are built from temporary entities that are deleted again. The world is then marked as
// modified; do not save it (answer No when Workbench asks on the next reload).
// Existing files are not overwritten.

[WorkbenchPluginAttribute(name: "Create Marx Sample Assets", category: "Marx", wbModules: { "WorldEditor" })]
class MRX_SampleAssetsPlugin : WorldEditorPlugin
{
	static const string TAG = "[MRX_PLUGIN] ";

	static const string CATALOG_FILE = "$Marx_Shop:Configs/Marx/Shop/MRX_SampleCatalog.conf";
	static const string SHOP_PREFAB_FILE = "$Marx_Shop:Prefabs/Marx/Shop/MRX_ShopTable.et";
	static const ResourceName SHOP_BASE_PREFAB = "{090B9013CF776866}Prefabs/Props/Furniture/TableLong_01/TableLong_01_base.et";
	static const string STASH_PREFAB_FILE = "$Marx_Stash:Prefabs/Marx/Stash/MRX_StashWardrobe.et";
	static const ResourceName STASH_BASE_PREFAB = "{FD47FF9699A52E82}Prefabs/Props/Furniture/Wardrobe_01.et";
	static const string STASH_CONTAINER_FILE = "$Marx_Stash:Prefabs/Marx/Stash/MRX_StashContainer.et";
	static const string ARSENAL_PREFAB_FILE = "$Marx_Shop:Prefabs/Marx/Shop/MRX_ArsenalBox.et";
	static const ResourceName ARSENAL_BASE_PREFAB = "{54986385A6AF77B2}Prefabs/Props/Military/Arsenal/ArsenalBoxes/ArsenalBox_Base.et";

	//! Keeps created container resources alive until the plugin finishes.
	protected ref array<ref Resource> m_aHolders = {};

	//------------------------------------------------------------------------------------------------
	override void Run()
	{
		Print(TAG + "sample assets start");
		ResourceName catalog = CreateCatalog();
		if (!catalog.IsEmpty())
		{
			map<string, string> shopValues = new map<string, string>();
			shopValues.Set("m_sShopId", "sample");
			shopValues.Set("m_sCatalog", catalog);
			CreateInteractivePrefab(SHOP_PREFAB_FILE, SHOP_BASE_PREFAB, "MRX_ShopComponent", shopValues, "MRX_Shop", "MRX_OpenShopAction", "#MRX-Shop_Trade", "0 0.9 0", false);
			CreateArsenalPrefab(catalog);
		}

		// The wardrobe mesh spans x -1.5..0, y 0..2, z 0..0.76 from its origin: the action sits in its middle.
		CreateInteractivePrefab(STASH_PREFAB_FILE, STASH_BASE_PREFAB, "MRX_StashPointComponent", new map<string, string>(), "MRX_Stash", "MRX_OpenStashAction", "#MRX-Stash_Title", "-0.75 1 0.38", true);
		CreateStashContainerPrefab();
		Print(TAG + "sample assets done");
	}

	//------------------------------------------------------------------------------------------------
	protected ResourceName CreateCatalog()
	{
		string absPath;
		Workbench.GetAbsolutePath(CATALOG_FILE, absPath, false);
		if (FileIO.FileExists(absPath))
		{
			Print(TAG + "already exists, not overwritten: " + absPath, LogLevel.WARNING);
			return GetResourceName(absPath);
		}

		BaseContainer root = CreateContainer("MRX_ShopCatalog", string.Empty);
		BaseContainerList items = root.SetObjectArray("m_aItems");
		if (!items)
		{
			Print(TAG + "m_aItems FAILED", LogLevel.ERROR);
			return ResourceName.Empty;
		}

		AddItem(items, "field_dressing", "{A81F501D3EF6F38E}Prefabs/Items/Medicine/FieldDressing_01/FieldDressing_US_01.et", "Medical", 20);
		AddItem(items, "tourniquet", "{D70216B1B2889129}Prefabs/Items/Medicine/Tourniquet_01/Tourniquet_US_01.et", "Medical", 40);
		AddItem(items, "morphine", "{0D9A5DCF89AE7AA9}Prefabs/Items/Medicine/MorphineInjection_01/MorphineInjection_01.et", "Medical", 60);
		AddItem(items, "map", "{13772C903CB5E4F7}Prefabs/Items/Equipment/Maps/PaperMap_01_folded.et", "Equipment", 30);
		AddItem(items, "compass", "{61D4F80E49BF9B12}Prefabs/Items/Equipment/Compass/Compass_SY183.et", "Equipment", 50);
		AddItem(items, "watch", "{6FD6C96121905202}Prefabs/Items/Equipment/Watches/Watch_Vostok.et", "Equipment", 80);
		AddItem(items, "binoculars", "{0CF54B9A85D8E0D4}Prefabs/Items/Equipment/Binoculars/Binoculars_M22/Binoculars_M22.et", "Equipment", 150);

		FileIO.MakeDirectory(FilePath.StripFileName(absPath));
		if (!BaseContainerTools.SaveContainer(root, ResourceName.Empty, CATALOG_FILE))
		{
			Print(TAG + "SaveContainer failed: " + CATALOG_FILE, LogLevel.ERROR);
			return ResourceName.Empty;
		}

		ResourceManager resourceManager = Workbench.GetModule(ResourceManager);
		if (!resourceManager.RegisterResourceFile(absPath, false))
			Print(TAG + "RegisterResourceFile failed: " + absPath, LogLevel.ERROR);

		ResourceName resourceName = GetResourceName(absPath);
		Print(TAG + "saved " + resourceName);
		return resourceName;
	}

	//------------------------------------------------------------------------------------------------
	protected void AddItem(BaseContainerList items, string id, ResourceName prefab, string category, int price)
	{
		BaseContainer item = CreateContainer("MRX_ShopItem", NewId());
		Log("item " + id, item.Set("m_sId", id) && item.Set("m_sPrefab", prefab) && item.Set("m_sCategory", category) && item.Set("m_iPrice", price));
		items.Insert(item);
	}

	//------------------------------------------------------------------------------------------------
	//! A child prefab of basePrefab with a Marx component and one user action in its own action context.
	protected void CreateInteractivePrefab(string file, ResourceName basePrefab, string componentClass, notnull map<string, string> componentValues, string contextName, string actionClass, string actionName, string actionOffset, bool enableReplication)
	{
		string absPath;
		Workbench.GetAbsolutePath(file, absPath, false);
		if (FileIO.FileExists(absPath))
		{
			Print(TAG + "already exists, not overwritten: " + absPath, LogLevel.WARNING);
			return;
		}

		WorldEditorAPI api = SCR_WorldEditorToolHelper.GetWorldEditorAPI();
		if (!api || !api.GetWorld())
		{
			Print(TAG + "open a world first", LogLevel.ERROR);
			return;
		}

		FileIO.MakeDirectory(FilePath.StripFileName(absPath));
		bool manageAction = !api.IsDoingEditAction();
		if (manageAction)
			api.BeginEntityAction("Marx sample prefab");

		IEntitySource source = api.CreateEntity(basePrefab, string.Empty, api.GetCurrentEntityLayerId(), null, vector.Zero, vector.Zero);
		if (!source)
		{
			Print(TAG + "CreateEntity failed: " + basePrefab, LogLevel.ERROR);
			if (manageAction)
				api.EndEntityAction();

			return;
		}

		source.ClearVariable("coords");

		array<ref ContainerIdPathEntry> componentPath = { new ContainerIdPathEntry(componentClass) };
		Log(componentClass, api.CreateComponent(source, componentClass) != null);
		foreach (string key, string value : componentValues)
		{
			Log(key, api.SetVariableValue(source, componentPath, key, value));
		}

		if (enableReplication)
			Log("RplComponent.Enabled", api.SetVariableValue(source, { new ContainerIdPathEntry("RplComponent") }, "Enabled", "1"));

		array<ref ContainerIdPathEntry> actionsPath = { new ContainerIdPathEntry("ActionsManagerComponent") };
		Log("ActionsManagerComponent", api.CreateComponent(source, "ActionsManagerComponent") != null);

		Log("ActionContexts", api.CreateObjectArrayVariableMember(source, actionsPath, "ActionContexts", "UserActionContext", 0));
		array<ref ContainerIdPathEntry> contextPath = { new ContainerIdPathEntry("ActionsManagerComponent"), new ContainerIdPathEntry("ActionContexts", 0) };
		Log("ContextName", api.SetVariableValue(source, contextPath, "ContextName", contextName));
		Log("Radius", api.SetVariableValue(source, contextPath, "Radius", "2"));
		Log("Omnidirectional", api.SetVariableValue(source, contextPath, "Omnidirectional", "1"));
		Log("Position", api.CreateObjectVariableMember(source, contextPath, "Position", "PointInfo"));
		array<ref ContainerIdPathEntry> positionPath = { new ContainerIdPathEntry("ActionsManagerComponent"), new ContainerIdPathEntry("ActionContexts", 0), new ContainerIdPathEntry("Position") };
		Log("Offset", api.SetVariableValue(source, positionPath, "Offset", actionOffset));

		Log("additionalActions", api.CreateObjectArrayVariableMember(source, actionsPath, "additionalActions", actionClass, 0));
		array<ref ContainerIdPathEntry> actionPath = { new ContainerIdPathEntry("ActionsManagerComponent"), new ContainerIdPathEntry("additionalActions", 0) };
		Log("ParentContextList", api.SetVariableValue(source, actionPath, "ParentContextList", contextName));
		Log("UIInfo", api.CreateObjectVariableMember(source, actionPath, "UIInfo", "UIInfo"));
		array<ref ContainerIdPathEntry> uiInfoPath = { new ContainerIdPathEntry("ActionsManagerComponent"), new ContainerIdPathEntry("additionalActions", 0), new ContainerIdPathEntry("UIInfo") };
		Log("UIInfo.Name", api.SetVariableValue(source, uiInfoPath, "Name", actionName));

		Log("CreateEntityTemplate", api.CreateEntityTemplate(source, absPath));
		api.DeleteEntity(source);
		if (manageAction)
			api.EndEntityAction();

		Print(TAG + "saved " + GetResourceName(absPath));
	}

	//------------------------------------------------------------------------------------------------
	//! A child prefab of the vanilla arsenal box that sells the sample catalog for Marx money, without saved loadouts.
	protected void CreateArsenalPrefab(ResourceName catalog)
	{
		string absPath;
		Workbench.GetAbsolutePath(ARSENAL_PREFAB_FILE, absPath, false);
		if (FileIO.FileExists(absPath))
		{
			Print(TAG + "already exists, not overwritten: " + absPath, LogLevel.WARNING);
			return;
		}

		WorldEditorAPI api = SCR_WorldEditorToolHelper.GetWorldEditorAPI();
		if (!api || !api.GetWorld())
		{
			Print(TAG + "open a world first", LogLevel.ERROR);
			return;
		}

		FileIO.MakeDirectory(FilePath.StripFileName(absPath));
		bool manageAction = !api.IsDoingEditAction();
		if (manageAction)
			api.BeginEntityAction("Marx sample arsenal");

		IEntitySource source = api.CreateEntity(ARSENAL_BASE_PREFAB, string.Empty, api.GetCurrentEntityLayerId(), null, vector.Zero, vector.Zero);
		if (!source)
		{
			Print(TAG + "CreateEntity failed: " + ARSENAL_BASE_PREFAB, LogLevel.ERROR);
			if (manageAction)
				api.EndEntityAction();

			return;
		}

		source.ClearVariable("coords");

		array<ref ContainerIdPathEntry> shopPath = { new ContainerIdPathEntry("MRX_ShopComponent") };
		Log("MRX_ShopComponent", api.CreateComponent(source, "MRX_ShopComponent") != null);
		Log("m_sShopId", api.SetVariableValue(source, shopPath, "m_sShopId", "sample_arsenal"));
		Log("m_sDisplayName", api.SetVariableValue(source, shopPath, "m_sDisplayName", "#MRX-Shop_Arsenal"));
		Log("m_sCatalog", api.SetVariableValue(source, shopPath, "m_sCatalog", catalog));
		Log("MRX_ArsenalShopComponent", api.CreateComponent(source, "MRX_ArsenalShopComponent") != null);

		// SCR_EArsenalSaveType.SAVING_DISABLED: loadouts saved at a Marx arsenal would respawn with unpaid gear.
		array<ref ContainerIdPathEntry> arsenalPath = { new ContainerIdPathEntry("SCR_ArsenalComponent") };
		Log("m_eArsenalSaveType", api.SetVariableValue(source, arsenalPath, "m_eArsenalSaveType", SCR_EArsenalSaveType.SAVING_DISABLED.ToString()));

		Log("CreateEntityTemplate", api.CreateEntityTemplate(source, absPath));
		api.DeleteEntity(source);
		if (manageAction)
			api.EndEntityAction();

		Print(TAG + "saved " + GetResourceName(absPath));
	}

	//------------------------------------------------------------------------------------------------
	//! A plain entity without model: replication, the stash storage and the stash container manager.
	protected void CreateStashContainerPrefab()
	{
		string absPath;
		Workbench.GetAbsolutePath(STASH_CONTAINER_FILE, absPath, false);
		if (FileIO.FileExists(absPath))
		{
			Print(TAG + "already exists, not overwritten: " + absPath, LogLevel.WARNING);
			return;
		}

		WorldEditorAPI api = SCR_WorldEditorToolHelper.GetWorldEditorAPI();
		if (!api || !api.GetWorld())
		{
			Print(TAG + "open a world first", LogLevel.ERROR);
			return;
		}

		FileIO.MakeDirectory(FilePath.StripFileName(absPath));
		bool manageAction = !api.IsDoingEditAction();
		if (manageAction)
			api.BeginEntityAction("Marx stash container");

		IEntitySource source = api.CreateEntity("GenericEntity", string.Empty, api.GetCurrentEntityLayerId(), null, vector.Zero, vector.Zero);
		if (!source)
		{
			Print(TAG + "CreateEntity GenericEntity failed", LogLevel.ERROR);
			if (manageAction)
				api.EndEntityAction();

			return;
		}

		source.ClearVariable("coords");
		Log("RplComponent", api.CreateComponent(source, "RplComponent") != null);

		IEntityComponentSource storage = api.CreateComponent(source, "MRX_StashStorageComponent");
		Log("MRX_StashStorageComponent", storage != null);
		array<ref ContainerIdPathEntry> storagePath = { new ContainerIdPathEntry("MRX_StashStorageComponent") };
		// Without a model the storage's own size is tiny, so the capacity comes from these values, not from the size.
		Log("UseCapacityCoefficient", api.SetVariableValue(source, storagePath, "UseCapacityCoefficient", "0"));
		Log("MaxCumulativeVolume", api.SetVariableValue(source, storagePath, "MaxCumulativeVolume", "200000"));
		Log("MaxItemSize", api.SetVariableValue(source, storagePath, "MaxItemSize", "300 300 300"));
		Log("m_fMaxWeight", api.SetVariableValue(source, storagePath, "m_fMaxWeight", "1000"));
		Log("Attributes", api.CreateObjectVariableMember(source, storagePath, "Attributes", "SCR_ItemAttributeCollection"));
		array<ref ContainerIdPathEntry> attributesPath = { new ContainerIdPathEntry("MRX_StashStorageComponent"), new ContainerIdPathEntry("Attributes") };
		Log("ItemDisplayName", api.CreateObjectVariableMember(source, attributesPath, "ItemDisplayName", "UIInfo"));
		array<ref ContainerIdPathEntry> namePath = { new ContainerIdPathEntry("MRX_StashStorageComponent"), new ContainerIdPathEntry("Attributes"), new ContainerIdPathEntry("ItemDisplayName") };
		Log("ItemDisplayName.Name", api.SetVariableValue(source, namePath, "Name", "#MRX-Stash_Title"));
		Log("ItemPhysAttributes", api.CreateObjectVariableMember(source, attributesPath, "ItemPhysAttributes", "ItemPhysicalAttributes"));

		Log("MRX_StashContainerManagerComponent", api.CreateComponent(source, "MRX_StashContainerManagerComponent") != null);

		Log("CreateEntityTemplate", api.CreateEntityTemplate(source, absPath));
		api.DeleteEntity(source);
		if (manageAction)
			api.EndEntityAction();

		Print(TAG + "saved " + GetResourceName(absPath));
	}

	//------------------------------------------------------------------------------------------------
	protected ResourceName GetResourceName(string absPath)
	{
		ResourceManager resourceManager = Workbench.GetModule(ResourceManager);
		MetaFile meta = resourceManager.GetMetaFile(absPath);
		if (!meta)
			return ResourceName.Empty;

		return meta.GetResourceID();
	}

	//------------------------------------------------------------------------------------------------
	protected BaseContainer CreateContainer(string className, string name)
	{
		Resource holder = BaseContainerTools.CreateContainer(className);
		if (!holder || !holder.IsValid())
		{
			Print(TAG + "CreateContainer failed: " + className, LogLevel.ERROR);
			return null;
		}

		BaseContainer container = holder.GetResource().ToBaseContainer();
		if (!name.IsEmpty())
			container.SetName(name);

		m_aHolders.Insert(holder);
		return container;
	}

	//------------------------------------------------------------------------------------------------
	protected string NewId()
	{
		string id = Workbench.GenerateGloballyUniqueID64();
		if (!id.StartsWith("{"))
			id = "{" + id + "}";

		return id;
	}

	//------------------------------------------------------------------------------------------------
	protected void Log(string step, bool result)
	{
		if (result)
			Print(TAG + step + " ok");
		else
			Print(TAG + step + " FAILED", LogLevel.ERROR);
	}
}
