import bpy
import json
import os


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
FILES = [
    'DarkSciFi_ModularAssetPack.blend',
    os.path.join('Libraries', 'Humanoid_Modular.blend'),
    os.path.join('Libraries', 'Void_Quadruped.blend'),
    os.path.join('Libraries', 'Ship_ModularKit.blend'),
    os.path.join('Libraries', 'Space_Asteroids.blend'),
    os.path.join('Libraries', 'Projectile_Meshes.blend'),
]

EXPECTED = {
    'Humanoid_Modular.blend': ['HUM_Walk_Loop', 'HUM_Run_Loop', 'HUM_Turn_Left_90', 'HUM_Turn_Right_90', 'HUM_Attack_Melee', 'HUM_Jump', 'HUM_Walk_To_Run', 'HUM_Run_To_Walk', 'HUM_Pose_Turn_Neg45', 'HUM_Pose_Turn_Zero', 'HUM_Pose_Turn_Pos45', 'HUM_Aim_Rifle_Enter', 'HUM_Aim_Rifle_Idle', 'HUM_Aim_Rifle_Fire', 'HUM_Death_Fall'],
    'Void_Quadruped.blend': ['VOID_Walk_Loop', 'VOID_Run_Loop', 'VOID_Turn_Left_90', 'VOID_Turn_Right_90', 'VOID_Attack_Pounce', 'VOID_Attack_Claw', 'VOID_Jump', 'VOID_Walk_To_Run', 'VOID_Run_To_Walk', 'VOID_Pose_Turn_Neg45', 'VOID_Pose_Turn_Zero', 'VOID_Pose_Turn_Pos45', 'VOID_Death_Collapse'],
    'Projectile_Meshes.blend': ['PROJ_Rifle_Ballistic_Spin', 'PROJ_Rifle_ArmorPiercing_Spin', 'PROJ_Energy_PlasmaBolt_Spin'],
}
SKELETAL_EXPECTED = {'Humanoid_Modular.blend', 'Void_Quadruped.blend'}
EXPECTED_OBJECTS = {
    'Projectile_Meshes.blend': ['PROJ_Rifle_Ballistic', 'PROJ_Rifle_ArmorPiercing', 'PROJ_Energy_PlasmaBolt', 'PROJ_Rifle_SpentCasing'],
}


report = {'status': 'PASS', 'files': {}}
for rel in FILES:
    path = os.path.join(ROOT, rel)
    bpy.ops.wm.open_mainfile(filepath=path, load_ui=False)
    meshes = [o for o in bpy.data.objects if o.type == 'MESH']
    armatures = [o for o in bpy.data.objects if o.type == 'ARMATURE']
    actions = sorted(a.name for a in bpy.data.actions)
    skinned = [o.name for o in meshes if any(m.type == 'ARMATURE' and m.object for m in o.modifiers)]
    missing = [name for name in EXPECTED.get(os.path.basename(rel), []) if name not in actions]
    object_names = {o.name for o in bpy.data.objects}
    missing_objects = [name for name in EXPECTED_OBJECTS.get(os.path.basename(rel), []) if name not in object_names]
    entry = {
        'size_bytes': os.path.getsize(path),
        'objects': len(bpy.data.objects),
        'meshes': len(meshes),
        'armatures': len(armatures),
        'materials': len(bpy.data.materials),
        'actions': len(actions),
        'action_names': actions,
        'skeletal_mesh_parts': len(skinned),
        'missing_required_actions': missing,
        'missing_required_objects': missing_objects,
    }
    if missing or missing_objects or (os.path.basename(rel) in SKELETAL_EXPECTED and (not armatures or not skinned)):
        entry['status'] = 'FAIL'
        report['status'] = 'FAIL'
    else:
        entry['status'] = 'PASS'
    report['files'][rel.replace('\\', '/')] = entry

out = os.path.join(ROOT, 'Documentation', 'ValidationReport.json')
with open(out, 'w', encoding='utf-8') as f:
    json.dump(report, f, ensure_ascii=False, indent=2)
print('VALIDATION_STATUS', report['status'])
print(json.dumps(report, ensure_ascii=False))
if report['status'] != 'PASS':
    raise SystemExit(2)
