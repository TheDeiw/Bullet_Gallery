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
