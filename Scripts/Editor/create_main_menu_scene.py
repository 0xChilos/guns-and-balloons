"""Build the 3D scene of the main menu (phase 1: scene only, no widget yet).

Creates:
  /Game/Scenes/MainMenu                    - small, non-partitioned level
  /Game/UI/MainMenu/BP_MenuBalloon         - display-only balloon (SK_Balloon, no gameplay components)
  /Game/UI/MainMenu/BP_MainMenuGameMode    - game mode without a pawn (the level camera takes the view)
  /Game/UI/MainMenu/LS_MenuBalloonIdle     - looping additive bob/sway for the balloon

The look (filter volume, sky, sun, fog, clouds) is recreated from the actors in /Game/PostProcess
(same classes, same settings), so the menu uses the same MI_OilyDepths instance as the game.

Run inside the open editor (console):  py "<project>/Scripts/Editor/create_main_menu_scene.py"
Results are written to Saved/MainMenuSetup.log. Rerunning rebuilds the menu actors in the
existing level. /Game/PostProcess is only read, never modified or saved.
"""
import math
import os
import re

import unreal

LOOK_LEVEL = "/Game/PostProcess"
MENU_LEVEL = "/Game/Scenes/MainMenu"
MENU_DIR = "/Game/UI/MainMenu"
BALLOON_MESH = "/Game/Assets/Balloon/SK_Balloon"
LOOK_ACTOR_LABELS = [
    "PPV_OilyDepths", "SM_SkySphere", "SkyLight", "SkyAtmosphere",
    "DirectionalLight", "ExponentialHeightFog", "VolumetricCloud",
]

# Framing: TBD placeholders, tune by eye.
BALLOON_LOCATION = unreal.Vector(0.0, 0.0, 0.0)
# The propeller is on the balloon's local +Y; this yaw points it away from the camera, behind the basket.
BALLOON_ROTATION = unreal.Rotator(roll=0.0, pitch=0.0, yaw=-76.0)
CAMERA_LOCATION = unreal.Vector(-3000.0, -750.0, 520.0)
CAMERA_ROTATION = unreal.Rotator(roll=0.0, pitch=2.0, yaw=0.0)
CAMERA_FOV = 50.0

# Menu-only atmosphere (the game's look actors are copied unchanged, then these overrides apply).
# All TBD placeholders, tune by eye.
FOG_VOLUMETRIC = dict(enable_volumetric_fog=True, volumetric_fog_scattering_distribution=0.6,
                      volumetric_fog_extinction_scale=2.0, volumetric_fog_distance=20000.0,
                      volumetric_fog_start_distance=1500.0)
SUN_VOLUMETRIC_SCATTERING = 2.0
# Night: the copied sun becomes a cool moon (bright enough to keep the balloon readable);
# sky light and fog glow go down with it.
NIGHT = dict(moon_intensity=0.65, moon_color=unreal.LinearColor(0.6, 0.72, 0.9, 1.0),
             sky_light_intensity=0.15,
             # distance fog takes the night horizon's teal, so everything fades into mist, not black
             fog_color=unreal.LinearColor(0.045, 0.085, 0.085, 1.0), fog_directional_scale=0.5)
# Night sky: a menu-only instance of the engine sky sphere material (the copied SM_SkySphere uses
# /Engine/EngineSky/M_Sky_Panning_Clouds2, which ignores the scene's sun).
SKY_PARENT = "/Engine/EngineSky/M_Sky_Panning_Clouds2"
NIGHT_SKY_VECTORS = {
    "Zenith Color": unreal.LinearColor(0.004, 0.012, 0.018, 1.0),
    "Horizon color": unreal.LinearColor(0.06, 0.11, 0.11, 1.0),
    "Cloud color": unreal.LinearColor(0.03, 0.04, 0.05, 0.0),
    "Sun color": unreal.LinearColor(0.85, 0.9, 0.8, 1.0),          # the moon disc
    "Light direction": unreal.LinearColor(-1.0, -0.3, -0.22, 1.0),  # moon up and to the right
}
NIGHT_SKY_SCALARS = {"Stars brightness": 1.5, "Sun brightness": 3.0, "Sun Radius": 0.0009}
# Searchlights from just below the frame, narrow and soft, grazing the balloon: someone is
# watching it. (label suffix, location, aim point). Intensity in candelas (project light units).
SEARCHLIGHTS = [  # behind the camera, out of frame: only the beams reach the balloon
    ("Left", unreal.Vector(-4500.0, -3800.0, -1500.0), unreal.Vector(0.0, -150.0, 800.0)),
    ("Right", unreal.Vector(-4000.0, 3200.0, -1200.0), unreal.Vector(0.0, 200.0, 400.0)),
]
SEARCHLIGHT = dict(intensity=3000.0, color=unreal.LinearColor(1.0, 0.92, 0.8, 1.0),
                   outer_cone=4.0, inner_cone=0.5, radius=20000.0, volumetric=4.0)
# Small warm glow of the stove, attached to the balloon so it bobs with it.
STOVE_GLOW = dict(offset=unreal.Vector(0.0, 0.0, 60.0), intensity=8.0, radius=450.0,
                  color=unreal.LinearColor(1.0, 0.45, 0.15, 1.0), volumetric=1.0)
# Background that breaks the horizon, hidden in the smoke: visible, but only as veiled shapes.
# (mesh, (x, y), yaw, scale). Glow slots (furnace, jammer warning lights) show through the haze.
CITY = "/Game/Assets/Locations/City/"
FOREST = "/Game/Assets/Locations/Forest/"
SKYLINE_BASE_Z = -1200.0
SKYLINE = [
    (CITY + "SM_Factory_B", (17000.0, -5500.0), -20.0, 1.0),
    (CITY + "SM_JammerTower", (24000.0, 300.0), 15.0, 1.0),
    (CITY + "SM_Factory_B", (21000.0, 11000.0), 160.0, 0.9),
    # pine forest at several depths (35.6 m at scale 1): bases in the smoke, upper parts above it
    (FOREST + "SM_Pine_Tall_B", (14000.0, -9500.0), 0.0, 1.1),
    (FOREST + "SM_Pine_Tall_B", (16000.0, -7200.0), 70.0, 1.0),
    (FOREST + "SM_Pine_Tall_B", (12500.0, -11500.0), 150.0, 1.05),
    (FOREST + "SM_Pine_Tall_B", (18500.0, -9000.0), 250.0, 1.15),
    (FOREST + "SM_Pine_Tall_B", (19000.0, -3000.0), 30.0, 1.0),
    (FOREST + "SM_Pine_Tall_B", (21500.0, -1200.0), 190.0, 1.2),
    (FOREST + "SM_Pine_Tall_B", (20000.0, 2800.0), 110.0, 1.05),
    (FOREST + "SM_Pine_Tall_B", (15000.0, 8500.0), 210.0, 1.1),
    (FOREST + "SM_Pine_Tall_B", (17500.0, 6200.0), 300.0, 1.0),
    (FOREST + "SM_Pine_Tall_B", (13500.0, 12500.0), 20.0, 1.3),
    (FOREST + "SM_Pine_Tall_B", (22500.0, 7500.0), 330.0, 1.15),
]
# Smoke the background hides in: thin, faintly glowing banks (lit by their own emissive, so at
# night they read as mist, not as dark blobs). (location, uniform scale; ~5 m radius at scale 1).
HIDING_SMOKE = [
    ((16000.0, -6000.0, -2600.0), 10.0),   # left factory: low, the chimney top stays above it
    ((23000.0, 500.0, 200.0), 13.0),       # the jammer's base
    ((19000.0, 9500.0, -2600.0), 10.0),    # right factory and pines: low, chimney top above
    ((13500.0, -11000.0, -3000.0), 10.0),  # left pines: low, their tops stay above it
    ((20000.0, 2500.0, -3000.0), 10.0),    # middle pines: low, their tops stay above it
    ((8000.0, -6000.0, -1800.0), 12.0),    # low mist in front, below the horizon
    ((9000.0, 3000.0, -1800.0), 12.0),
]
# Low smoke band along the horizon left of the balloon: swallows the factory bodies and the pine
# trunks; chimney tops, pine tops and the jammer mast stay above it. Dense at the bottom, fading up.
SMOKE_BAND = [
    ((13000.0, -12000.0, -1600.0), 7.0),
    ((14500.0, -8500.0, -1600.0), 7.0),
    ((16000.0, -5000.0, -1600.0), 7.0),
    ((17500.0, -1500.0, -1600.0), 7.0),
    ((19000.0, 2000.0, -1600.0), 7.0),
]
SMOKE_BAND_PROPS = dict(radial_fog_extinction=0.8, height_fog_extinction=0.8, height_fog_falloff=300.0,
                        fog_albedo=unreal.LinearColor(0.4, 0.45, 0.42, 1.0),
                        fog_emissive=unreal.LinearColor(0.035, 0.055, 0.055, 1.0), fog_phase_g=0.3)
HIDING_SMOKE_PROPS = dict(radial_fog_extinction=0.5, height_fog_extinction=0.45, height_fog_falloff=400.0,
                          fog_albedo=unreal.LinearColor(0.4, 0.45, 0.42, 1.0),
                          fog_emissive=unreal.LinearColor(0.035, 0.055, 0.055, 1.0), fog_phase_g=0.3)
CHIMNEY_SMOKE = "/Game/Assets/Locations/Blueprints/NS_FactorySmoke"
CHIMNEY_SOCKETS = ["smoke_1", "smoke_2"]  # SM_Factory_A has both, SM_Factory_B only smoke_1

MENU_BRIGHTNESS = 0.45  # AMainMenuScreen::Brightness

# Idle motion: TBD placeholders. The loop is 24 s, so every period must divide 24.
LOOP_SECONDS = 24
BOB_HEIGHT_CM, BOB_PERIOD_S = 15.0, 8.0
ROLL_DEG, ROLL_PERIOD_S = 1.5, 12.0
YAW_DEG, YAW_PERIOD_S = 3.0, 24.0

LOG_PATH = os.path.join(unreal.Paths.project_saved_dir(), "MainMenuSetup.log")
_log = []


def log(msg):
    _log.append(str(msg))
    unreal.log("[MainMenuSetup] %s" % msg)


def flush():
    with open(LOG_PATH, "w") as f:
        f.write("\n".join(_log))


asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def create_blueprint(name, parent):
    path = "%s/%s" % (MENU_DIR, name)
    if eal.does_asset_exist(path):
        log("reuse %s" % path)
        return eal.load_asset(path)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent)
    bp = asset_tools.create_asset(name, MENU_DIR, None, factory)
    log("created %s" % path)
    return bp


def setup_menu_balloon():
    bp = create_blueprint("BP_MenuBalloon", unreal.SkeletalMeshActor)
    mesh = eal.load_asset(BALLOON_MESH)
    # Edit the inherited component template through the subobject system so it is saved with the BP.
    sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    template = None
    for handle in sds.k2_gather_subobject_data_for_blueprint(bp):
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_associated_object(
            unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle))
        if isinstance(obj, unreal.SkeletalMeshComponent):
            template = obj
            break
    if template is None:
        template = unreal.get_default_object(bp.generated_class()).skeletal_mesh_component
        log("subobject template not found, editing the CDO component")
    template.set_editor_property("skeletal_mesh_asset", mesh)
    # Reference pose only: ABP_Balloon looks for levers/stove that the menu balloon doesn't have.
    template.set_editor_property("animation_mode", unreal.AnimationMode.ANIMATION_SINGLE_NODE)
    template.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    eal.save_loaded_asset(bp)
    return bp


def setup_game_mode():
    bp = create_blueprint("BP_MainMenuGameMode", unreal.GameModeBase)
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property("default_pawn_class", None)
    controller = getattr(unreal, "MainMenuPlayerController", None)  # C++, needs the module rebuilt
    if controller:
        cdo.set_editor_property("player_controller_class", controller)
    else:
        log("MainMenuPlayerController not compiled in yet: game mode keeps the default controller")
    eal.save_loaded_asset(bp)
    return bp


# Properties that belong to the source level/hierarchy and must not be copied.
_SKIP_PROPS = re.compile(r"data_layer|hlod|runtime_grid|spatially_loaded|external|attach_|^relative_|"
                         r"guid|^on_|_delegate$|_tick$|^layers$|^replicated_movement$|^body_instance$|"
                         r"^root_component$|^actor_label$|^folder_path$|^parent_component$|^instigator$|^owner$")
_PROP_DOC = re.compile(r"^- ``(\w+)`` \(.*?\):\s*\[Read-Write\]", re.M)


_EDITOR_ONLY_COMPONENTS = (unreal.BillboardComponent, unreal.ArrowComponent)


def _copyable(value):
    if isinstance(value, (unreal.Actor, unreal.ActorComponent, unreal.Level, unreal.World)):
        return False
    if isinstance(value, unreal.Object):
        # Objects private to the source level (brush model, body setup...) must not be shared.
        return not value.get_outermost().get_name().startswith((LOOK_LEVEL, "/Temp/", "/Game/__External"))
    if isinstance(value, (list, unreal.Array)):
        return all(_copyable(v) for v in value)
    return True


def _same(a, b):
    try:
        return a == b
    except Exception:
        return False


def _detach(value):
    """Struct wrappers point into their owner's memory and die with the source level: copy them."""
    if isinstance(value, unreal.StructBase):
        return value.copy()
    if isinstance(value, (list, unreal.Array)):
        return [_detach(v) for v in value]
    return value


def snapshot(obj):
    """Read-write editor properties of obj (names parsed from the generated Python docs)."""
    values = {}
    for name in _PROP_DOC.findall(obj.__class__.__doc__ or ""):
        if _SKIP_PROPS.search(name):
            continue
        try:
            value = obj.get_editor_property(name)
        except Exception:
            continue
        if _copyable(value):
            values[name] = _detach(value)
    return values


def apply(obj, values):
    """Set only the values that differ from obj's current (default) values."""
    changed, failed = [], []
    for name, value in values.items():
        try:
            if _same(obj.get_editor_property(name), value):
                continue
            obj.set_editor_property(name, value)
            changed.append(name)
        except Exception:
            failed.append(name)
    return changed, failed


def read_look_actors():
    """Snapshot the look actors of LOOK_LEVEL. (Editor clipboard copy crashes when run from Python.)"""
    if les.get_current_level().get_outermost().get_name() != LOOK_LEVEL:
        les.load_level(LOOK_LEVEL)
    by_label = {a.get_actor_label(): a for a in eas.get_all_level_actors()}
    missing = [l for l in LOOK_ACTOR_LABELS if l not in by_label]
    if missing:
        raise RuntimeError("look actors missing in %s: %s" % (LOOK_LEVEL, missing))
    shots = []
    for label in LOOK_ACTOR_LABELS:
        actor = by_label[label]
        comps = {c.get_name(): snapshot(c) for c in actor.get_components_by_class(unreal.ActorComponent)
                 if not isinstance(c, _EDITOR_ONLY_COMPONENTS)}
        shots.append((label, actor.get_class(), actor.get_actor_transform(), snapshot(actor), comps))
    log("read %d look actors from %s" % (len(shots), LOOK_LEVEL))
    return shots


def spawn_look_actors(shots):
    for label, cls, transform, actor_props, comp_props in shots:
        actor = eas.spawn_actor_from_class(cls, transform.translation, transform.rotation.rotator())
        actor.set_actor_label(label)
        actor.set_actor_scale3d(transform.scale3d)
        changed, failed = apply(actor, actor_props)
        for comp in actor.get_components_by_class(unreal.ActorComponent):
            if comp.get_name() in comp_props:
                c_changed, c_failed = apply(comp, comp_props[comp.get_name()])
                changed += ["%s.%s" % (comp.get_name(), n) for n in c_changed]
                failed += ["%s.%s" % (comp.get_name(), n) for n in c_failed]
        log("spawned %s (%s)\n   copied: %s\n   not copied: %s" % (label, cls.get_name(), changed, failed or "none"))


def setup_atmosphere():
    """Menu-only smoke and light shafts on top of the copied look actors."""
    by_label = {a.get_actor_label(): a for a in eas.get_all_level_actors()}
    fog = by_label["ExponentialHeightFog"].get_components_by_class(unreal.ExponentialHeightFogComponent)[0]
    for name, value in FOG_VOLUMETRIC.items():
        fog.set_editor_property(name, value)
    fog.set_editor_property("fog_inscattering_luminance", NIGHT["fog_color"])
    c = fog.get_editor_property("directional_inscattering_luminance")
    k = NIGHT["fog_directional_scale"]
    fog.set_editor_property("directional_inscattering_luminance", unreal.LinearColor(c.r * k, c.g * k, c.b * k, c.a))
    sun = by_label["DirectionalLight"].get_components_by_class(unreal.DirectionalLightComponent)[0]
    sun.set_editor_property("volumetric_scattering_intensity", SUN_VOLUMETRIC_SCATTERING)
    sun.set_editor_property("intensity", NIGHT["moon_intensity"])
    sun.set_light_color(NIGHT["moon_color"])
    sky = by_label["SkyLight"].get_components_by_class(unreal.SkyLightComponent)[0]
    sky.set_editor_property("intensity", NIGHT["sky_light_intensity"])

    sky_sphere = by_label["SM_SkySphere"].get_components_by_class(unreal.StaticMeshComponent)[0]
    sky_sphere.set_material(0, night_sky_material())

    for suffix, location, target in SEARCHLIGHTS:
        look_at = unreal.MathLibrary.find_look_at_rotation(location, target)
        spot = eas.spawn_actor_from_class(unreal.SpotLight, location, look_at)
        spot.set_actor_label("MenuSearchlight_" + suffix)
        lc = spot.get_components_by_class(unreal.SpotLightComponent)[0]
        lc.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
        lc.set_editor_property("intensity", SEARCHLIGHT["intensity"])
        lc.set_light_color(SEARCHLIGHT["color"])
        lc.set_editor_property("outer_cone_angle", SEARCHLIGHT["outer_cone"])
        lc.set_editor_property("inner_cone_angle", SEARCHLIGHT["inner_cone"])
        lc.set_editor_property("attenuation_radius", SEARCHLIGHT["radius"])
        lc.set_editor_property("volumetric_scattering_intensity", SEARCHLIGHT["volumetric"])
        lc.set_editor_property("cast_volumetric_shadow", True)

    smoke_system = eal.load_asset(CHIMNEY_SMOKE)
    smokes = 0
    for i, (mesh_path, xy, yaw, scale) in enumerate(SKYLINE):
        actor = spawn_mesh("MenuSkyline_%02d" % i, mesh_path,
                           unreal.Vector(xy[0], xy[1], SKYLINE_BASE_Z), yaw, scale)
        comp = actor.static_mesh_component
        for socket in CHIMNEY_SOCKETS:
            if comp.does_socket_exist(socket):
                smoke = eas.spawn_actor_from_object(smoke_system, comp.get_socket_location(socket))
                smoke.set_actor_label("MenuChimneySmoke_%02d_%s" % (i, socket))
                smokes += 1
    for prefix, banks, props in (("MenuHidingSmoke", HIDING_SMOKE, HIDING_SMOKE_PROPS),
                                 ("MenuSmokeBand", SMOKE_BAND, SMOKE_BAND_PROPS)):
        for i, (loc, scale) in enumerate(banks):
            bank = eas.spawn_actor_from_class(unreal.LocalFogVolume, unreal.Vector(*loc))
            bank.set_actor_label("%s_%02d" % (prefix, i))
            bank.set_actor_scale3d(unreal.Vector(scale, scale, scale))
            comp = bank.get_components_by_class(unreal.LocalFogVolumeComponent)[0]
            for name, value in props.items():
                comp.set_editor_property(name, value)
    log("atmosphere: night sky, %d searchlights, %d background meshes, %d chimney smokes, %d smoke banks"
        % (len(SEARCHLIGHTS), len(SKYLINE), smokes, len(HIDING_SMOKE) + len(SMOKE_BAND)))


def spawn_mesh(label, mesh_path, location, yaw, scale):
    actor = eas.spawn_actor_from_class(unreal.StaticMeshActor, location, unreal.Rotator(0.0, 0.0, yaw))
    actor.set_actor_label(label)
    actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
    comp = actor.static_mesh_component
    comp.set_static_mesh(eal.load_asset(mesh_path))
    comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    return actor


def night_sky_material():
    """MI_MenuNightSky: the engine sky material with night colours, stars and a moon disc."""
    path = "%s/MI_MenuNightSky" % MENU_DIR
    if eal.does_asset_exist(path):
        mi = eal.load_asset(path)
    else:
        mi = asset_tools.create_asset("MI_MenuNightSky", MENU_DIR, unreal.MaterialInstanceConstant,
                                      unreal.MaterialInstanceConstantFactoryNew())
    mel = unreal.MaterialEditingLibrary
    mel.set_material_instance_parent(mi, eal.load_asset(SKY_PARENT))
    for name, value in NIGHT_SKY_VECTORS.items():
        mel.set_material_instance_vector_parameter_value(mi, name, value)
    for name, value in NIGHT_SKY_SCALARS.items():
        mel.set_material_instance_scalar_parameter_value(mi, name, value)
    mel.update_material_instance(mi)
    eal.save_loaded_asset(mi)
    return mi


def setup_stove_glow(balloon):
    mesh = balloon.skeletal_mesh_component
    location = mesh.get_socket_location("stove") + STOVE_GLOW["offset"]
    glow = eas.spawn_actor_from_class(unreal.PointLight, location)
    glow.set_actor_label("MenuStoveGlow")
    lc = glow.get_components_by_class(unreal.PointLightComponent)[0]
    lc.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    lc.set_editor_property("intensity", STOVE_GLOW["intensity"])
    lc.set_editor_property("attenuation_radius", STOVE_GLOW["radius"])
    lc.set_editor_property("volumetric_scattering_intensity", STOVE_GLOW["volumetric"])
    lc.set_light_color(STOVE_GLOW["color"])
    glow.attach_to_component(mesh, "stove", unreal.AttachmentRule.KEEP_WORLD,
                             unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD, False)


def sine_keys(channel, amplitude, period_s, fps):
    """Keys at quarter periods; auto tangents turn them into a near-sine loop."""
    steps = int(round(LOOP_SECONDS / period_s * 4))
    for i in range(steps + 1):
        t = i * period_s / 4.0
        channel.add_key(unreal.FrameNumber(int(round(t * fps))), amplitude * math.sin(i * math.pi / 2.0))


def setup_idle_sequence(balloon_actor):
    path = "%s/LS_MenuBalloonIdle" % MENU_DIR
    if eal.does_asset_exist(path):
        seq = eal.load_asset(path)  # reuse and clear: deleting a referenced asset can prompt
        for old in seq.get_bindings():
            old.remove()
    else:
        seq = asset_tools.create_asset("LS_MenuBalloonIdle", MENU_DIR, unreal.LevelSequence,
                                       unreal.LevelSequenceFactoryNew())
    fps = seq.get_display_rate().numerator / float(seq.get_display_rate().denominator)
    end = int(LOOP_SECONDS * fps)
    seq.set_playback_start(0)
    seq.set_playback_end(end)
    binding = seq.add_possessable(balloon_actor)
    track = binding.add_track(unreal.MovieScene3DTransformTrack)
    section = track.add_section()
    section.set_range(0, end)
    section.set_blend_type(unreal.MovieSceneBlendType.ADDITIVE)  # offsets on top of the placed transform
    # Channel names carry an index suffix ("Location.Z_0").
    ch = {re.sub(r"_\d+$", "", c.get_name()): c for c in section.get_all_channels()}
    sine_keys(ch["Location.Z"], BOB_HEIGHT_CM, BOB_PERIOD_S, fps)
    sine_keys(ch["Rotation.X"], ROLL_DEG, ROLL_PERIOD_S, fps)
    sine_keys(ch["Rotation.Z"], YAW_DEG, YAW_PERIOD_S, fps)
    eal.save_loaded_asset(seq)

    player = eas.spawn_actor_from_class(unreal.LevelSequenceActor, unreal.Vector(0, 0, 0))
    player.set_actor_label("LSA_MenuBalloonIdle")
    player.set_sequence(seq)
    settings = player.get_editor_property("playback_settings")
    loop = settings.get_editor_property("loop_count")
    loop.set_editor_property("value", -1)
    settings.set_editor_property("loop_count", loop)
    try:
        settings.set_editor_property("auto_play", True)
    except Exception:
        player.set_editor_property("auto_play", True)
    player.set_editor_property("playback_settings", settings)
    log("idle sequence: %d frames at %.0f fps, looping" % (end, fps))


def is_menu_actor(actor):
    """Actors this script owns in MENU_LEVEL (rebuilt on every run)."""
    label = actor.get_actor_label()
    return label in LOOK_ACTOR_LABELS or label.startswith("Menu") or label == "LSA_MenuBalloonIdle"


def main():
    current = les.get_current_level().get_outermost().get_name()
    dirty = [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages() +
             unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()]
    if current == MENU_LEVEL and MENU_LEVEL in dirty:
        les.save_current_level()  # our own level: save so switching maps doesn't prompt
        dirty.remove(MENU_LEVEL)
    for own in [p for p in dirty if p.startswith(MENU_DIR + "/")]:
        eal.save_asset(own, only_if_is_dirty=True)  # leftovers of a previous run of this script
        dirty.remove(own)
    if dirty:
        raise RuntimeError("unsaved packages in the editor, save or discard first: %s" % dirty)
    if not eal.does_directory_exist(MENU_DIR):
        eal.make_directory(MENU_DIR)

    balloon_bp = setup_menu_balloon()
    game_mode_bp = setup_game_mode()
    shots = read_look_actors()  # read-only: LOOK_LEVEL is never modified or saved

    if eal.does_asset_exist(MENU_LEVEL):
        les.load_level(MENU_LEVEL)
        old = [a for a in eas.get_all_level_actors() if is_menu_actor(a)]
        eas.destroy_actors(old)
        log("rebuilding %s: removed %d old menu actors" % (MENU_LEVEL, len(old)))
    elif not les.new_level(MENU_LEVEL, False):
        raise RuntimeError("new_level failed for %s" % MENU_LEVEL)
    spawn_look_actors(shots)
    setup_atmosphere()

    balloon = eas.spawn_actor_from_class(balloon_bp.generated_class(), BALLOON_LOCATION, BALLOON_ROTATION)
    balloon.set_actor_label("MenuBalloon")
    setup_stove_glow(balloon)

    camera = eas.spawn_actor_from_class(unreal.CameraActor, CAMERA_LOCATION, CAMERA_ROTATION)
    camera.set_actor_label("MenuCamera")
    camera.camera_component.set_editor_property("field_of_view", CAMERA_FOV)
    camera.camera_component.set_editor_property("constrain_aspect_ratio", False)
    camera.set_editor_property("auto_activate_for_player", unreal.AutoReceiveInput.PLAYER0)

    setup_idle_sequence(balloon)

    screen_class = getattr(unreal, "MainMenuScreen", None)  # C++, needs the module rebuilt
    if screen_class:
        # It places itself in front of the active camera at runtime; the spawn point doesn't matter.
        screen = eas.spawn_actor_from_class(screen_class, CAMERA_LOCATION, CAMERA_ROTATION)
        screen.set_actor_label("MenuScreen")
        # The scene's exposure and tonemapper brighten the unlit panel too; this keeps the menu's
        # amber / parchment / dim states apart. TBD.
        screen.set_editor_property("brightness", MENU_BRIGHTNESS)
    else:
        log("MainMenuScreen not compiled in yet: no menu panel placed")

    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    world.get_world_settings().set_editor_property("default_game_mode", game_mode_bp.generated_class())

    les.save_current_level()
    log("saved %s" % MENU_LEVEL)


try:
    main()
    log("DONE")
except Exception as e:
    import traceback
    log("FAILED: %s\n%s" % (e, traceback.format_exc()))
finally:
    flush()
