import bpy
import os

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUTPUT = os.path.join(ROOT, "SimpleGame", "Data", "RuntimeMeshes.txt")


def is_humanoid_arm_part(name):
    return any(part in name for part in (
        "Body_UpperArm", "Body_ForeArm", "Body_Hand",
        "Armor_Shoulder", "Armor_Forearm"))


GROUPS = {
    "humanoid_soldier": lambda name: name.startswith("HUM_") and not name.startswith("HUM_Weapon_") and "Base_B" not in name and "Scavenger" not in name,
    "humanoid_armed_body": lambda name: name.startswith("HUM_") and not name.startswith("HUM_Weapon_") and "Base_B" not in name and "Scavenger" not in name and not is_humanoid_arm_part(name),
    "void_quadruped": lambda name: name.startswith("VOID_") and not name.startswith("VOID_Blade_Fore_") and not name.startswith("VOID_Scythe_"),
    "void_claw_left": lambda name: (name.startswith("VOID_Blade_Fore_") or name.startswith("VOID_Scythe_")) and name.endswith("_L"),
    "void_claw_right": lambda name: (name.startswith("VOID_Blade_Fore_") or name.startswith("VOID_Scythe_")) and name.endswith("_R"),
    "ship_wall": lambda name: name.startswith("SHIP_Wall_Ribbed") or name.startswith("SHIP_Wall_Rib_"),
    "ship_console": lambda name: name.startswith("SHIP_Console_"),
    "ship_medbed": lambda name: name.startswith("SHIP_MedBed_") or name.startswith("SHIP_MedScanner_"),
    "ship_power": lambda name: name.startswith("SHIP_Device_Power") or name.startswith("SHIP_PowerJunction_"),
    "ship_engine": lambda name: name.startswith("SHIP_Engine_"),
    "ship_coolant": lambda name: name.startswith("SHIP_Coolant_") or name.startswith("SHIP_Device_Oxygen"),
    "ship_pilot_seat": lambda name: name.startswith("SHIP_PilotSeat_"),
    "ship_prop_cargo": lambda name: name.startswith("SHIPPROP_CargoCrate_"),
    "ship_prop_locker": lambda name: name.startswith("SHIPPROP_Locker_"),
    "ship_prop_workbench": lambda name: name.startswith("SHIPPROP_Workbench_"),
    "ship_prop_bunk": lambda name: name.startswith("SHIPPROP_Bunk_"),
    "ship_prop_galley": lambda name: name.startswith("SHIPPROP_Galley_"),
    "ship_prop_air_scrubber": lambda name: name.startswith("SHIPPROP_AirScrubber_"),
    "ship_prop_vent": lambda name: name.startswith("SHIPPROP_Vent_"),
    "ship_prop_pipe_rack": lambda name: name.startswith("SHIPPROP_PipeRack_"),
    "ship_prop_emergency": lambda name: name.startswith("SHIPPROP_Emergency_") or name.startswith("SHIPPROP_Extinguisher_"),
    "ship_prop_shelf": lambda name: name.startswith("SHIPPROP_Shelf_") or name.startswith("SHIPPROP_SupplyBin"),
    "ship_prop_cable_reel": lambda name: name.startswith("SHIPPROP_CableReel_"),
    "ship_prop_ceiling_light": lambda name: name.startswith("SHIPPROP_CeilingLight_"),
    "asteroid_a": lambda name: name.startswith("SPACE_Asteroid_A"),
    "asteroid_b": lambda name: name.startswith("SPACE_Asteroid_B"),
    "asteroid_c": lambda name: name.startswith("SPACE_Asteroid_C"),
    "asteroid_d": lambda name: name.startswith("SPACE_Asteroid_D"),
    "pulse_rifle_mesh": lambda name: name.startswith("HUM_Weapon_PulseRifle_"),
    "projectile_ballistic": lambda name: name == "PROJ_Rifle_Ballistic",
    "projectile_armor_piercing": lambda name: name == "PROJ_Rifle_ArmorPiercing",
    "projectile_plasma_bolt": lambda name: name == "PROJ_Energy_PlasmaBolt",
    "projectile_spent_casing": lambda name: name == "PROJ_Rifle_SpentCasing",
    "replacement_medical_terminal": lambda name: name.startswith("REPL_MedTerminal_") and "Hologram" not in name,
    "replacement_medical_hologram": lambda name: name == "REPL_MedTerminal_Hologram",
    "replacement_weapon_arms": lambda name: name.startswith("REPL_Arm_") and name != "REPL_Arm_TestWeapon",
    "replacement_aux_engine": lambda name: name.startswith("REPL_AuxEngine_") and "Impeller" not in name and "Blade" not in name,
    "replacement_aux_impeller": lambda name: "REPL_AuxEngine_Impeller" in name or name.startswith("REPL_AuxEngine_Blade_"),
    "replacement_magazine": lambda name: name.startswith("REPL_Magazine_"),
    "replacement_magazine_effect": lambda name: name.startswith("REPL_MagPickup_"),
    "replacement_document_unread": lambda name: name.startswith("REPL_StoryDocument_Unread_"),
    "replacement_document_read": lambda name: name.startswith("REPL_StoryDocument_Read_"),
    "replacement_player_ship": lambda name: name.startswith("REPL_PlayerShip_") or (name.startswith("REPL_Ships_") and name.endswith("_55.0")),
    "replacement_noctis_ship": lambda name: name.startswith("REPL_Noctis_") or (name.startswith("REPL_Ships_") and name.endswith("_59.0")),
    "replacement_erebus_ship": lambda name: name.startswith("REPL_Erebus_"),
    "replacement_cargo_ship": lambda name: name.startswith("REPL_CargoShip_") or (name.startswith("REPL_Ships_") and name.endswith("_67.0")),
    "replacement_patrol_ship": lambda name: name.startswith("REPL_PatrolShip_") or (name.startswith("REPL_Ships_") and name.endswith("_71.0")),
    "replacement_station": lambda name: name.startswith("REPL_Station_"),
    "replacement_debris_hull": lambda name: name.startswith("REPL_Debris_Broken") or name == "REPL_Debris_WarningPlate_0",
    "replacement_debris_engine": lambda name: name.startswith("REPL_Debris_Engine") or name == "REPL_Debris_WarningPlate_1",
    "replacement_debris_antenna": lambda name: name.startswith("REPL_Debris_Antenna") or name == "REPL_Debris_WarningPlate_2",
    "replacement_debris_cargo": lambda name: name.startswith("REPL_Debris_Cargo") or name == "REPL_Debris_WarningPlate_3",
    "replacement_debris_solar": lambda name: name.startswith("REPL_Debris_Solar") or name == "REPL_Debris_WarningPlate_4",
    "replacement_debris_infected": lambda name: name.startswith("REPL_Debris_Infected") or name == "REPL_Debris_WarningPlate_5",
    "replacement_docking_structure": lambda name: name.startswith("REPL_Dock_") and not name.startswith("REPL_DockedPlayerShip_"),
    "replacement_infected_structure": lambda name: name.startswith("REPL_Infected_"),
    "replacement_light_pylons": lambda name: name.startswith("REPL_LightPylon_"),
    "replacement_docked_player_ship": lambda name: name.startswith("REPL_DockedPlayerShip_") or name.startswith("REPL_DockedShip_Engine_") or name.startswith("REPL_DockedShip_Glow_"),
}


def color_for(material):
    if material is None:
        return (0.35, 0.40, 0.43)
    color = material.diffuse_color
    return (color[0], color[1], color[2])


with open(OUTPUT, "w", encoding="utf-8") as output:
    output.write("# Blender-authored runtime triangle meshes\n")
    for group_name, predicate in GROUPS.items():
        output.write(f"mesh {group_name}\n")
        for source in bpy.data.objects:
            if source.type != "MESH" or not predicate(source.name):
                continue
            mesh = source.data
            mesh.calc_loop_triangles()
            matrix = source.matrix_world
            for triangle in mesh.loop_triangles:
                material = None
                if triangle.material_index < len(source.data.materials):
                    material = source.data.materials[triangle.material_index]
                red, green, blue = color_for(material)
                points = [matrix @ mesh.vertices[index].co for index in triangle.vertices]
                if group_name.startswith("projectile_"):
                    points = [point.__class__((point.x, -point.z, point.y)) for point in points]
                values = " ".join(f"{value:.5f}" for point in points for value in point)
                output.write(f"tri {values} {red:.4f} {green:.4f} {blue:.4f}\n")

print(f"Exported runtime meshes to {OUTPUT}")
