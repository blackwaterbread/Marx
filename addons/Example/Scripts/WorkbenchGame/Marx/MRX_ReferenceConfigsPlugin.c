// Development tool: creates the Marx reference persistence and systems configs in Marx_Core.
// Object IDs come from Workbench.GenerateGloballyUniqueID64(); resource GUIDs from resource registration.

[WorkbenchPluginAttribute(name: "Create Marx Reference Configs", category: "Marx", wbModules: { "WorldEditor" })]
class MRX_ReferenceConfigsPlugin : WorldEditorPlugin
{
	static const string TAG = "[MRX_PLUGIN] ";

	static const ResourceName PARENT_PERSISTENCE = "{B76E7F1AF7A5D00C}Configs/Systems/Persistence/GameMode/GameMaster.conf";
	static const ResourceName PARENT_SYSTEMS = "{8DDC2A311929D52F}Configs/Systems/GameMasterSystems.conf";
	//! "Main" save game database declared in vanilla Configs/Systems/Persistence/BaseSetup.conf.
	static const string MAIN_DATABASE_ID = "{6624ADA9A88DA0F6}";
	//! SCR_PersistenceSystem entry declared in vanilla GameMasterSystems.conf.
	static const string PARENT_PERSISTENCE_SYSTEM_ID = "{65DC893A38833A14}";

	static const string PERSISTENCE_FILE = "$Marx_Core:Configs/Marx/Persistence/MRX_GameMasterPersistence.conf";
	static const string SYSTEMS_FILE = "$Marx_Core:Configs/Marx/Systems/MRX_GameMasterSystems.conf";

	//! Keeps created container resources alive until the plugin finishes.
	protected ref array<ref Resource> m_aHolders = {};

	//------------------------------------------------------------------------------------------------
	override void Run()
	{
		Print(TAG + "start");
		ResourceName persistence = CreatePersistenceConfig();
		if (persistence.IsEmpty())
			return;

		AddStashCollection(PERSISTENCE_FILE);
		CreateSystemsConfig(persistence);
		Print(TAG + "done");
	}

	//------------------------------------------------------------------------------------------------
	//! Adds the stash collection and its state config next to the wallet ones, keeping the existing IDs.
	//! A text edit, because the container API would also write the inherited vanilla entries into the file.
	protected void AddStashCollection(string file)
	{
		string absPath;
		Workbench.GetAbsolutePath(file, absPath, true);
		FileHandle reader = FileIO.OpenFile(absPath, FileMode.READ);
		if (!reader)
		{
			Print(TAG + "cannot read " + absPath, LogLevel.ERROR);
			return;
		}

		array<string> lines = {};
		string line;
		while (reader.ReadLine(line) >= 0)
		{
			lines.Insert(line);
		}

		reader.Close();

		int walletName = -1;
		int walletSerializer = -1;
		foreach (int i, string text : lines)
		{
			if (text.Contains("\"" + MRX_NativeBackend.STASH_COLLECTION_NAME + "\""))
			{
				Print(TAG + "stash collection already present");
				return;
			}

			if (text.Contains("Name \"" + MRX_NativeBackend.COLLECTION_NAME + "\""))
				walletName = i;

			if (text.Contains("Serializer MRX_WalletStateSerializer"))
				walletSerializer = i;
		}

		// Expected layout: Name, Storage, "  }" / Serializer, "       }", "      }".
		if (walletName < 0 || walletSerializer < 0 || walletSerializer + 2 >= lines.Count() || !lines[walletName + 1].Contains("Storage "))
		{
			Print(TAG + "unexpected persistence config layout, stash collection not added", LogLevel.ERROR);
			return;
		}

		string storageLine = lines[walletName + 1];
		string storageId = storageLine.Substring(storageLine.IndexOf("\"") + 1, 18);
		string collectionId = NewId();

		// Insert the later block first so the earlier index stays valid.
		lines.InsertAt("      }", walletSerializer + 3);
		lines.InsertAt("       }", walletSerializer + 3);
		lines.InsertAt("       Serializer MRX_StashStateSerializer \"" + NewId() + "\" {", walletSerializer + 3);
		lines.InsertAt("       Collection \"" + collectionId + "\"", walletSerializer + 3);
		lines.InsertAt("      StatePersistenceConfig \"" + NewId() + "\" {", walletSerializer + 3);

		lines.InsertAt("  }", walletName + 3);
		lines.InsertAt("   Storage \"" + storageId + "\"", walletName + 3);
		lines.InsertAt("   Name \"" + MRX_NativeBackend.STASH_COLLECTION_NAME + "\"", walletName + 3);
		lines.InsertAt("  PersistenceCollection \"" + collectionId + "\" {", walletName + 3);

		FileHandle writer = FileIO.OpenFile(absPath, FileMode.WRITE);
		if (!writer)
		{
			Print(TAG + "cannot write " + absPath, LogLevel.ERROR);
			return;
		}

		foreach (string outLine : lines)
		{
			writer.WriteLine(outLine);
		}

		writer.Close();
		Print(TAG + "stash collection added to " + absPath);
	}

	//------------------------------------------------------------------------------------------------
	protected ResourceName CreatePersistenceConfig()
	{
		BaseContainer root = CreateContainer("PersistenceSystemConfig", string.Empty);
		root.SetAncestor(PARENT_PERSISTENCE);

		string storageId = NewId();
		BaseContainer storage = CreateContainer("GamemodeStorage", storageId);
		Log("storage.Database", storage.Set("Database", MAIN_DATABASE_ID));
		Log("Storages", root.SetObjectArray("Storages").Insert(storage));

		string collectionId = NewId();
		BaseContainer collection = CreateContainer("PersistenceCollection", collectionId);
		Log("collection.Name", collection.Set("Name", MRX_NativeBackend.COLLECTION_NAME));
		Log("collection.Storage", collection.Set("Storage", storageId));
		Log("Collections", root.SetObjectArray("Collections").Insert(collection));

		BaseContainer stateConfig = CreateContainer("StatePersistenceConfig", NewId());
		Log("state.Collection", stateConfig.Set("Collection", collectionId));
		Log("state.Serializer", stateConfig.SetObject("Serializer", CreateContainer("MRX_WalletStateSerializer", NewId())));

		BaseContainer gameplay = CreateContainer("PersistenceConfigGroup", "Gameplay");
		Log("Gameplay.Configurations", gameplay.SetObjectArray("Configurations").Insert(stateConfig));

		BaseContainer scriptedStates = CreateContainer("PersistenceConfigGroup", "ScriptedStates");
		Log("ScriptedStates.Configurations", scriptedStates.SetObjectArray("Configurations").Insert(gameplay));
		Log("Configurations", root.SetObjectArray("Configurations").Insert(scriptedStates));

		return SaveAndRegister(root, PERSISTENCE_FILE);
	}

	//------------------------------------------------------------------------------------------------
	protected void CreateSystemsConfig(ResourceName persistence)
	{
		BaseContainer root = CreateContainer("SystemSettings", string.Empty);
		root.SetAncestor(PARENT_SYSTEMS);

		BaseContainer system = CreateContainer("SCR_PersistenceSystem", PARENT_PERSISTENCE_SYSTEM_ID);
		Log("system.Config", system.Set("Config", persistence));
		Log("Systems", root.SetObjectArray("Systems").Insert(system));

		SaveAndRegister(root, SYSTEMS_FILE);
	}

	//------------------------------------------------------------------------------------------------
	protected ResourceName SaveAndRegister(BaseContainer root, string file)
	{
		string absPath;
		Workbench.GetAbsolutePath(file, absPath, false);
		if (FileIO.FileExists(absPath))
		{
			Print(TAG + "already exists, not overwritten: " + absPath, LogLevel.WARNING);
			return GetResourceName(absPath);
		}

		FileIO.MakeDirectory(FilePath.StripFileName(absPath));
		if (!BaseContainerTools.SaveContainer(root, ResourceName.Empty, file))
		{
			Print(TAG + "SaveContainer failed: " + file, LogLevel.ERROR);
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
