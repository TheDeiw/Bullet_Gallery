# Bullet Gallery: museum complex (UE 5.8)

A labs project for the "Game Programming" course: a museum complex with a first-person character and shooting.

- Engine: **Unreal Engine 5.8**, C++ project (module `Bullet_Gallery`)
- Main level: `Content/_MuseumProject/Maps/L_MuseumComplex` (World Partition)
- GameMode: `Content/_MuseumProject/MyGameMode`, character: `MyPlayerCharacter` (C++ `APlayerCharacter`)

## What is (and isn't) in the repository

The repository holds **only the code and our own assets**:
`Source/`, `Config/`, `Bullet_Gallery.uproject`, `Content/_MuseumProject`, the level's actors
(`__ExternalActors__`, `__ExternalObjects__`), `Input`, and Epic template content (`Characters`, `ThirdPerson`, ...).

Binary assets (`.uasset`, `.umap`) are stored in **Git LFS**.

**Third-party asset packs are not in the repository** (because of their size, 19 GB, and their licenses).
The level uses these packs, so add them to `Content/` after cloning:

| Folder in `Content/` | Where it comes from |
|---|---|
| `GalleryPack/` | Art Gallery pack (Fab / Epic). The `ArtGalleryAssets 5.8` project → Migrate the `GalleryPack` folder |
| `UnrealBucket/` | Unreal Bucket Free Pack (Fab) |
| `Fab/Modern_Staircase_Collection_3D_Model_Pack/` | Modern Staircase Collection 3D Model Pack (Fab) |
| `CityPark/` | City Park pack (Fab). Lab 4 decor: statues, café kiosk, table, chairs, umbrella, bushes, ivy |
| `Imported_Assets/NationalMaritimeMuseum/` | National Maritime Museum scans (Fab). Lab 4 decor: ship's wheel, charts, astrolabe, diving helmets, ... |

Unused packs (`FAB_Assets`, `DemoProject`) aren't needed.

> After adding the staircase pack, open the `Wooden_Stairs` mesh and set
> **Collision → Collision Complexity = Use Complex Collision As Simple**,
> otherwise you can't walk up the stairs (the mesh's default collision is a single solid convex hull).

> Lab 4 changed the collision of some `GalleryPack` meshes (the obstacles on floor 1 show different collision types).
> After adding the pack, set them again in the Static Mesh Editor:
> `SM_Display_Stand_01a`, `SM_Display_Stand_01c`, `SM_Display_Stand_01d` → one box (Remove Collision, Add Box Simplified Collision);
> `SM_Statue_01a` → one capsule; `SM_Statue_03` → **Collision Complexity = Use Complex Collision As Simple**.

## Controls

WASD move, mouse look, LMB fire, RMB aim, Shift walk, R reload, Space jump,
**E** pick up / drop a light physics object (LMB while carrying throws it).
Walk into heavy crates and doors to push them. Medkits and ammo boxes are picked up by walking into them.
The blue teleport pads link the museum (floor 2) and the Art Gallery hall.
Glass doors (`BP_AutoGlassDoor`) open by themselves when you or an enemy come close; the three wooden doors are physics doors you push.
Wall colors come from `Content/_MuseumProject/Walls` (`M_WallTwoTone`: paint above, wood paneling below, world-aligned).

## Lab 5: AI night guard

- `BP_GuardNPC` (C++ `AGuardNPC`) is an NPC enemy driven by `BP_GuardAIController` and the behaviour tree
  `BT_MuseumGuard` with the Blackboard `BB_MuseumGuard` (all in `Content/_MuseumProject/AI`).
  It patrols, stops and looks around, runs and shoots when it sees the player, checks the last known location,
  and dodges shots (the farther the shooter, the higher the chance). The Lab 3 statues stay on the level.
- The player belongs to `PlayersTeam` (`IGenericTeamAgentInterface`), so the guard's sight reports only the player.
- Trigger zones: `BP_PoisonZone` slows the guard down (and is an expensive nav area), `BP_BlindZone` hides the player.
- The navigation mesh is built at runtime (`RuntimeGeneration=Dynamic` in `DefaultEngine.ini`, World Partition level);
  `NavMesh_Floor1` covers the first floor.
- Editor module `Bullet_GalleryEditor` rebuilds `BB_MuseumGuard` and `BT_MuseumGuard` with their editor graph:
  set `bBuildRequested` on `/Script/Bullet_GalleryEditor.Default__GuardBehaviorTreeBuilder` (it checks the flag twice a second).
  The Blueprint tasks `BTT_SetSpeed` and `BTT_MoveToEffectiveRange` must exist first.

## Lab 6: animations, weather, day and night

- **Reload** (`AM_Player_Reload` montage, `ABP_Player`, C++ `UPlayerAnimInstance`): the Mauser C96 is loaded from the top with a stripper clip
  (`SM_StripperClip`, `SM_ClipRound` in `Content/_MuseumProject/Lab6/Weapon`, modelled in Blender).
  The left hand is moved by two Two Bone IK nodes. Each shot has a muzzle flash (`NS_MuzzleFlash` + a short point light); the player's bullets fly straight
  (the random spread is commented out in `AMuseumProjectile`).
- **Guard weapon**: `ABP_Guard` (C++ `UGuardAnimInstance`) raises and lowers the revolver with `WeaponRaiseAlpha`.
- **Animated exhibits**: Newton's cradle and a pendulum clock (Blender → FBX → `Content/_MuseumProject/Lab6/Exhibits`), statues rotated by `RotatingMovementComponent`.
- **Weather**: `AWeatherController` (rain showers, thunderstorm, wind gusts through `MPC_Weather`); the rain `NS_Rain` is a fixed area above the garden,
  so it is visible from inside the museum through the glass doors.
- **Day and night**: `ADayNightCycle` turns the sun by the hour (240 s per day, starts at 16:00), switches on a bluish moon at night and the lamps
  tagged `NightLight`. It only rotates the sun and toggles lights, so it does not conflict with the weather controller.
- **Garden** (`Lab6/Park`) is as big as the museum (68 × 42 m); the walkable area is limited by three invisible `BlockingVolume`s behind the hedge.

> `SK_MauserC96Pistol` comes from the UnrealBucket pack (not in the repository). After adding the pack, add the socket `ClipGuide` on the bone `body`
> (location 0, −19.61, 6.64; roll −108.445°), otherwise the reload clip has nowhere to attach.

## Release: the wave game (v1.0)

The level is now a complete game loop built in C++ (no Blueprint changes, nothing placed in the level):

- `UMuseumDirector` (`MuseumDirector.cpp`) is a world subsystem that runs the whole game: main menu over a camera flight,
  eight waves of a night (dusk to dawn, it drives `ADayNightCycle`), spawning around the player (navigation mesh inside the museum,
  a floor-traced ring in the garden), score and combo, perks between the waves, pause, game over and victory, hit stop, loot drops.
  Enemy kinds and waves are tables at the top of `BuildWaves` / `MakeVariant`.
- `UMuseumUIWidget` + `MuseumStyle.h`: every menu and the new HUD parts are Slate code (palette, buttons, sliders in `MuseumStyle.h`).
- `UMuseumUserSettings` (a `UGameUserSettings`, class set in `DefaultEngine.ini`): graphics (window mode, resolution, vsync, frame cap,
  quality groups, resolution scale), audio (master / music / effects through a sound mix), gameplay options. Saved in `GameUserSettings.ini`.
- `UMuseumSaveGame`: best score, best wave, victories (slot `MuseumProgress`).
- `AEnemy::ApplyVariant`, `AGuardNPC::ApplyVariant`: tougher, faster or bigger versions of the same Blueprints (sprinter, bronze sentinel, brute, the Curator).
- Player: recoil, head bob, landing dip, hit marker, floating damage numbers, damage direction arcs, perks (`ApplyPerk`).
- Test commands (not in shipping): `mg.start`, `mg.wave N`, `mg.kill`, `mg.god 1`, `mg.page main|settings|howto|pause|perks|gameover|victory`,
  `mg.win`, `mg.lose`, `mg.shot name`, `mg.wait seconds`; they can be given on the command line: `-ExecCmds="mg.start;mg.wave 3;mg.wait 4;mg.shot a"`.

### Building the release (Windows, Shipping)

```powershell
& "<UE_5.8>\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="<path>\Bullet_Gallery.uproject" -noP4 -platform=Win64 `
  -clientconfig=Shipping -serverconfig=Shipping -cook -build -stage -pak -iostore -compressed -prereqs -archive `
  -archivedirectory="<out>" -map=/Game/_MuseumProject/Maps/L_MuseumComplex -NoCompileEditor -utf8output
```

The cook takes 15-40 minutes (cold shader cache). The itch.io images, page text and the zip script live in `Bullet_Gallery_Release/` next to this folder.
Project icon and splash: `Build/Windows/Application.ico`, `Build/Windows/Splash/Splash.bmp` (generated by `Bullet_Gallery_Release/tools/make_icon.py`).
Defaults for the first launch (High quality, borderless window, vsync) are in `Config/DefaultGameUserSettings.ini`; `r.Lumen.AsyncCompute=0` in `DefaultEngine.ini` avoids a GPU hang seen on an RTX 4050 laptop.

## How to clone and run

```bash
git lfs install
git clone https://github.com/TheDeiw/Bullet_Gallery.git
```

1. Copy the packs from the table above into `Content/` (Fab → Add to Project, or Migrate from another project).
2. Right-click `Bullet_Gallery.uproject` → **Generate Visual Studio project files**.
3. Open the `.uproject` (UE 5.8) and agree to build the module, or build `Bullet_Gallery.sln` (Development Editor, Win64) from Rider / Visual Studio.

## Everyday workflow

```bash
git status
git add -A
git commit -m "Short description of the changes"
git push
```

- Save everything in the editor before committing (File → Save All).
- If you add a new folder of **your own** to `Content/`, add a `!Content/<Folder>/` line to `.gitignore`, otherwise git won't see it.
- The LFS limit on a free GitHub account is 10 GiB of storage and 10 GiB of downloads per month.
