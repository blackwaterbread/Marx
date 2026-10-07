// Publishes updates of the Marx mods to the Arma Reforger Workshop through Workbench's command line.
//
// Workbench publishes only updates this way (-publishAddon): release each mod once with Workbench > Publish Project
// and sign in there (Workbench > Link) first. Selected mods go out in dependency order (Core, UI, Shop, Stash), one
// Workbench run each, and the run stops at the first failure. Local-only folders (Workbench automation handlers in
// Scripts/WorkbenchGame/EnfusionMCP) are moved out of the addon for the run and put back afterwards.
//
//   bun tools/publish.ts --project Core,UI --version 0.1.1 --note "Fix the shop refund"
//   bun tools/publish.ts --project all --version 0.2.0 --note-file notes.txt
//   bun tools/publish.ts --project all --pack-only      packs into a temp folder instead (no upload)
//
// Options: --preview <image> and --screenshots <folder> replace the Workshop images; --dry-run prints the
// Workbench command lines without running anything.

import { spawn, spawnSync } from "node:child_process";
import { cpSync, existsSync, mkdirSync, openSync, readFileSync, readSync, closeSync, renameSync, rmSync, statSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { parseArgs } from "node:util";

// Addon folders under addons/, in dependency order.
const PROJECTS = ["Core", "UI", "Shop", "Stash"];
// Folders inside an addon that must never reach the Workshop.
const LOCAL_ONLY_DIRS = ["Scripts/WorkbenchGame/EnfusionMCP"];
// Everything this script and Workbench write goes to the temp folder: Windows' Controlled Folder Access may block
// bun from writing to Documents.
const WORK_ROOT = join(tmpdir(), "marx-publish");
const STASH_ROOT = join(WORK_ROOT, "stash");

const repoRoot = resolve(dirname(fileURLToPath(import.meta.url)), "..");

function fail(message: string): never
{
	console.error(message);
	process.exit(1);
}

// Runs a PowerShell command and returns its output as UTF-8 (paths may contain non-ASCII characters).
function powershell(command: string): string
{
	const result = spawnSync("powershell", ["-NoProfile", "-Command", `[Console]::OutputEncoding = New-Object Text.UTF8Encoding $false; ${command}`], { encoding: "utf8" });
	return (result.stdout ?? "").trim();
}

function steamLibraries(): string[]
{
	let steamRoot = "C:\\Program Files (x86)\\Steam";
	const reg = spawnSync("reg", ["query", "HKCU\\Software\\Valve\\Steam", "/v", "SteamPath"], { encoding: "utf8" });
	const match = /SteamPath\s+REG_SZ\s+(.+)/.exec(reg.stdout ?? "");
	if (match)
		steamRoot = match[1].trim();

	const libraries = [steamRoot];
	const vdf = join(steamRoot, "steamapps", "libraryfolders.vdf");
	if (existsSync(vdf))
	{
		for (const entry of readFileSync(vdf, "utf8").matchAll(/"path"\s+"([^"]+)"/g))
			libraries.push(entry[1].replaceAll("\\\\", "\\"));
	}

	return [...new Set(libraries)];
}

function findInSteamLibraries(relativePath: string): string | undefined
{
	return steamLibraries().map((library) => join(library, relativePath)).find((candidate) => existsSync(candidate));
}

function isOpenInWorkbench(gproj: string): boolean
{
	const commandLines = powershell("Get-CimInstance Win32_Process -Filter \"Name = 'ArmaReforgerWorkbenchSteamDiag.exe'\" | ForEach-Object { $_.CommandLine }");
	return commandLines.toLowerCase().includes(gproj.toLowerCase());
}

function moveDir(from: string, to: string)
{
	mkdirSync(dirname(to), { recursive: true });
	try
	{
		renameSync(from, to);
	}
	catch
	{
		// Different drive: copy, then delete.
		cpSync(from, to, { recursive: true });
		rmSync(from, { recursive: true, force: true });
	}
}

// Moves the addon's local-only folders to the stash and returns the folders moved.
function stashLocalOnly(project: string): string[]
{
	const moved: string[] = [];
	for (const dir of LOCAL_ONLY_DIRS)
	{
		const source = join(repoRoot, "addons", project, dir);
		if (!existsSync(source))
			continue;

		moveDir(source, join(STASH_ROOT, project, dir));
		moved.push(dir);
		console.log(`  moved out: ${dir}`);
	}

	return moved;
}

// Puts stashed folders back, including ones left behind by an interrupted earlier run.
function restoreLocalOnly(project: string)
{
	for (const dir of LOCAL_ONLY_DIRS)
	{
		const stashed = join(STASH_ROOT, project, dir);
		if (!existsSync(stashed))
			continue;

		const target = join(repoRoot, "addons", project, dir);
		if (existsSync(target))
			fail(`Both ${target} and ${stashed} exist. Keep one of them and run again.`);

		moveDir(stashed, target);
		console.log(`  restored: ${dir}`);
	}
}

// Prints new console.log lines about packing and publishing while Workbench runs.
function followLog(consoleLog: string): () => void
{
	let offset = 0;
	let partial = "";
	const timer = setInterval(() =>
	{
		if (!existsSync(consoleLog))
			return;

		const size = statSync(consoleLog).size;
		if (size <= offset)
			return;

		const buffer = Buffer.alloc(size - offset);
		const fd = openSync(consoleLog, "r");
		readSync(fd, buffer, 0, buffer.length, offset);
		closeSync(fd);
		offset = size;

		const lines = (partial + buffer.toString("utf8")).split(/\r?\n/);
		partial = lines.pop() ?? "";
		for (const line of lines)
		{
			// "<time> <category> : <message>"; paths in the message (logs, ResourceDB) would match too, so skip those.
			const separator = line.indexOf(": ");
			const category = line.slice(0, separator);
			const message = line.slice(separator + 2);
			if (separator < 0 || category.includes("SCRIPT") || /^(CLI Params|FileSystem|ResourceDB|Log )/.test(message))
				continue;

			if (/Packaging|[Pp]ublish|[Bb]undl|[Uu]pload|Workshop/.test(message) || (category.includes("BACKEND") && category.includes("(E)")))
				console.log(`  | ${line.trim()}`);
		}
	}, 1000);

	return () => clearInterval(timer);
}

function runWorkbench(exe: string, gameDir: string, args: string[], consoleLog: string): Promise<number | null>
{
	return new Promise((resolveExit) =>
	{
		const child = spawn(exe, args, { cwd: gameDir, stdio: "ignore" });
		console.log(`  Workbench started (pid ${child.pid}), waiting for it to exit...`);
		const stopFollowing = followLog(consoleLog);
		child.on("exit", (code) =>
		{
			stopFollowing();
			resolveExit(code);
		});
		child.on("error", (error) =>
		{
			stopFollowing();
			console.error(`  Could not start Workbench: ${error.message}`);
			resolveExit(null);
		});
	});
}

function printErrors(consoleLog: string)
{
	if (!existsSync(consoleLog))
		return;

	const text = readFileSync(consoleLog, "utf8");
	// Resource leak reports at shutdown are noise.
	const errors = text.split("==== Resource leaks ====")[0].split(/\r?\n/).filter((line) => line.includes("(E)"));
	for (const line of errors.slice(-20))
		console.log(`  ! ${line.trim()}`);
}

async function main()
{
	const { values } = parseArgs({
		options: {
			"project": { type: "string" },
			"version": { type: "string" },
			"note": { type: "string" },
			"note-file": { type: "string" },
			"preview": { type: "string" },
			"screenshots": { type: "string" },
			"pack-only": { type: "boolean", default: false },
			"dry-run": { type: "boolean", default: false },
		},
	});

	if (!values.project)
		fail(`Pass --project with ${PROJECTS.join(", ")} (comma-separated) or all.`);

	const requested = values.project.toLowerCase() === "all" ? PROJECTS : values.project.split(",").map((name) => name.trim());
	for (const name of requested)
	{
		if (!PROJECTS.includes(name))
			fail(`Unknown project ${name}. Known: ${PROJECTS.join(", ")}.`);
	}

	const projects = PROJECTS.filter((name) => requested.includes(name));
	const packOnly = values["pack-only"];

	let noteFile = "";
	if (!packOnly)
	{
		if (!values.version || !/^\d+\.\d+\.\d+$/.test(values.version) || values.version.split(".").some((part) => Number(part) > 32000))
			fail("Pass --version as major.minor.patch (each part up to 32000), higher than the published one.");

		if (values["note-file"])
		{
			noteFile = resolve(values["note-file"]);
			if (!existsSync(noteFile))
				fail(`Change note file not found: ${noteFile}`);
		}
		else if (values.note)
		{
			mkdirSync(WORK_ROOT, { recursive: true });
			noteFile = join(WORK_ROOT, "note.txt");
			writeFileSync(noteFile, values.note, "utf8");
		}
		else
		{
			fail("Pass --note or --note-file with the change notes.");
		}
	}

	const workbenchExe = findInSteamLibraries("steamapps\\common\\Arma Reforger Tools\\Workbench\\ArmaReforgerWorkbenchSteamDiag.exe");
	const gameDir = findInSteamLibraries("steamapps\\common\\Arma Reforger");
	if (!workbenchExe || !gameDir)
		fail("Workbench or Arma Reforger not found in the Steam libraries.");

	const addonDirs = PROJECTS.concat("Example").map((name) => join(repoRoot, "addons", name)).filter((dir) => existsSync(join(dir, "addon.gproj")));
	addonDirs.push(join(gameDir, "addons"));
	const now = new Date();
	const pad = (value: number) => String(value).padStart(2, "0");
	const stamp = `${now.getFullYear()}-${pad(now.getMonth() + 1)}-${pad(now.getDate())}_${pad(now.getHours())}-${pad(now.getMinutes())}-${pad(now.getSeconds())}`;

	for (const project of projects)
	{
		const gproj = join(repoRoot, "addons", project, "addon.gproj");
		const id = /^\s*ID\s+"([^"]+)"/m.exec(readFileSync(gproj, "utf8"))?.[1] ?? project;
		const logsDir = join(WORK_ROOT, "logs", `${stamp}_${id}`);
		const outDir = join(WORK_ROOT, packOnly ? "pack" : "bundle", id);

		const args = ["-gproj", gproj, "-addonsDir", addonDirs.join(","), "-logsDir", logsDir, "-scriptAuthorizeAll", "-wbModule=ResourceManager"];
		if (packOnly)
		{
			args.push("-packAddon", "-packAddonDir", outDir, "-packAddonCacheDir", join(WORK_ROOT, "pack-cache", id));
		}
		else
		{
			args.push("-publishAddon", "-publishAddonDir", outDir, "-publishAddonVersion", values.version!, "-publishAddonChangeNoteFile", noteFile);
			if (values.preview)
				args.push("-publishAddonPreviewImage", resolve(values.preview));
			if (values.screenshots)
				args.push("-publishAddonScreenshots", resolve(values.screenshots));
		}

		console.log(`\n== ${id} (${packOnly ? "pack only" : `publish ${values.version}`})`);
		if (values["dry-run"])
		{
			console.log(`  ${workbenchExe} ${args.map((arg) => (arg.includes(" ") ? `"${arg}"` : arg)).join(" ")}`);
			continue;
		}

		if (isOpenInWorkbench(gproj))
			fail(`  Workbench has ${gproj} open. Close it first.`);

		// Workbench does not create missing parent folders of the output folder.
		mkdirSync(logsDir, { recursive: true });
		if (packOnly)
			rmSync(outDir, { recursive: true, force: true });
		mkdirSync(outDir, { recursive: true });

		restoreLocalOnly(project);
		const moved = stashLocalOnly(project);

		// Workbench keeps running after Ctrl+C and may still be packing, so the folders stay out until the next run.
		const onInterrupt = () =>
		{
			console.log(`\n  Interrupted. Moved folders stay in ${join(STASH_ROOT, project)} until the next run.`);
			process.exit(130);
		};
		process.on("SIGINT", onInterrupt);

		const consoleLog = join(logsDir, "console.log");
		let code: number | null;
		try
		{
			code = await runWorkbench(workbenchExe, gameDir, args, consoleLog);
		}
		finally
		{
			process.off("SIGINT", onInterrupt);
			if (moved.length > 0)
				restoreLocalOnly(project);
		}

		const log = existsSync(consoleLog) ? readFileSync(consoleLog, "utf8") : "";
		if (packOnly)
		{
			const pak = join(outDir, "data.pak");
			if (!log.includes("Packaging project successful") || !existsSync(pak))
			{
				printErrors(consoleLog);
				fail(`  Packing failed (exit code ${code}). Log: ${consoleLog}`);
			}

			const contents = readFileSync(pak);
			const leaked = LOCAL_ONLY_DIRS.filter((dir) => contents.includes(dir.split("/").pop()!));
			console.log(`  ${pak} (${contents.length} bytes)${leaked.length > 0 ? `, still contains ${leaked.join(", ")}` : ""}`);
			if (leaked.length > 0)
				fail("  Local-only folders reached the pak.");
		}
		else if (!log.includes("Project uploaded successfully"))
		{
			printErrors(consoleLog);
			fail(`  Publishing not confirmed (exit code ${code}). Check the log: ${consoleLog}`);
		}
		else
		{
			console.log(`  Published ${id} ${values.version}.`);
		}
	}
}

await main();
