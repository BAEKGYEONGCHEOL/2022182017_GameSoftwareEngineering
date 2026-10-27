import bpy
import os

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUTPUT = os.path.join(ROOT, "SimpleGame", "Data", "RuntimeMeshes.txt")

GROUPS = {
    "humanoid_soldier": lambda name: name.startswith("HUM_") and not name.startswith("HUM_Weapon_") and "Base_B" not in name and "Scavenger" not in name,
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
    "asteroid_a": lambda name: name.startswith("SPACE_Asteroid_A"),
    "asteroid_b": lambda name: name.startswith("SPACE_Asteroid_B"),
    "asteroid_c": lambda name: name.startswith("SPACE_Asteroid_C"),
    "asteroid_d": lambda name: name.startswith("SPACE_Asteroid_D"),
    "pulse_rifle_mesh": lambda name: name.startswith("HUM_Weapon_PulseRifle_"),
    "projectile_ballistic": lambda name: name == "PROJ_Rifle_Ballistic",
    "projectile_armor_piercing": lambda name: name == "PROJ_Rifle_ArmorPiercing",
    "projectile_plasma_bolt": lambda name: name == "PROJ_Energy_PlasmaBolt",
    "projectile_spent_casing": lambda name: name == "PROJ_Rifle_SpentCasing",
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
