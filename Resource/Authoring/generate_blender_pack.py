import bpy
import math
import os
import sys
import json
from mathutils import Vector


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
LIB = os.path.join(ROOT, "Libraries")
PREVIEW = os.path.join(ROOT, "Previews")
DOCS = os.path.join(ROOT, "Documentation")
os.makedirs(LIB, exist_ok=True)
os.makedirs(PREVIEW, exist_ok=True)
os.makedirs(DOCS, exist_ok=True)


def clean():
    bpy.ops.object.mode_set(mode='OBJECT') if bpy.context.object and bpy.context.object.mode != 'OBJECT' else None
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    for block in (bpy.data.meshes, bpy.data.curves, bpy.data.armatures, bpy.data.materials,
                  bpy.data.cameras, bpy.data.lights, bpy.data.actions):
        for item in list(block):
            if item.users == 0:
                block.remove(item)


def mat(name, color, metallic=0.0, rough=0.55, emission=None):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.diffuse_color = (*color, 1.0)
    m.use_nodes = True
    bs = m.node_tree.nodes.get('Principled BSDF')
    bs.inputs['Base Color'].default_value = (*color, 1.0)
    bs.inputs['Metallic'].default_value = metallic
    bs.inputs['Roughness'].default_value = rough
    if emission:
        bs.inputs['Emission Color'].default_value = (*emission, 1.0)
        bs.inputs['Emission Strength'].default_value = 4.0
    return m


M = {}


def materials():
    global M
    M = {
        'skin_pale': mat('MAT_Human_Skin_Pale', (0.48, 0.38, 0.34), 0.0, 0.75),
        'skin_dark': mat('MAT_Human_Skin_Dark', (0.18, 0.12, 0.10), 0.0, 0.78),
        'cloth': mat('MAT_Cloth_Coal', (0.035, 0.045, 0.055), 0.05, 0.86),
        'armor': mat('MAT_Armor_Gunmetal', (0.075, 0.095, 0.105), 0.78, 0.33),
        'armor2': mat('MAT_Armor_Oxide', (0.18, 0.075, 0.035), 0.65, 0.48),
        'bone': mat('MAT_Void_Bone', (0.38, 0.35, 0.27), 0.05, 0.72),
        'void': mat('MAT_Void_Carapace', (0.105, 0.055, 0.14), 0.25, 0.42),
        'flesh': mat('MAT_Void_Flesh', (0.22, 0.045, 0.065), 0.0, 0.65),
        'cyan': mat('MAT_Emissive_Cyan', (0.01, 0.13, 0.17), 0.1, 0.25, (0.02, 0.65, 0.82)),
        'amber': mat('MAT_Emissive_Amber', (0.22, 0.08, 0.01), 0.1, 0.25, (1.0, 0.25, 0.015)),
        'red': mat('MAT_Emissive_Red', (0.18, 0.01, 0.015), 0.1, 0.28, (0.9, 0.015, 0.01)),
        'black': mat('MAT_Rubber_Black', (0.009, 0.012, 0.014), 0.15, 0.62),
        'steel': mat('MAT_Scratched_Steel', (0.18, 0.20, 0.21), 0.88, 0.28),
        'wall': mat('MAT_Hull_DarkMetal', (0.055, 0.065, 0.075), 0.82, 0.38),
        'trim': mat('MAT_Hull_Trim', (0.14, 0.15, 0.14), 0.72, 0.44),
        'medical': mat('MAT_Medical_Ceramic', (0.28, 0.32, 0.31), 0.3, 0.38),
        'rock': mat('MAT_Asteroid_Basalt', (0.055, 0.045, 0.04), 0.05, 0.94),
        'dust': mat('MAT_Asteroid_Dust', (0.19, 0.12, 0.075), 0.0, 1.0),
    }


def coll(name, parent=None):
    c = bpy.data.collections.new(name)
    (parent.children if parent else bpy.context.scene.collection.children).link(c)
    return c


def move_to(obj, collection):
    for c in list(obj.users_collection):
        c.objects.unlink(obj)
    collection.objects.link(obj)


def assign(obj, material):
    if material:
        obj.data.materials.append(material)
    return obj


def cube(name, loc, scale, material, collection, bevel=0.04, rot=(0, 0, 0)):
    bpy.ops.mesh.primitive_cube_add(location=loc, rotation=rot)
    o = bpy.context.object
    o.name = name
    o.scale = scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if bevel:
        mod = o.modifiers.new('LowPolyEdge', 'BEVEL')
        mod.width = bevel
        mod.segments = 1
    assign(o, material)
    move_to(o, collection)
    return o


def cyl(name, loc, radius, depth, material, collection, rot=(0, 0, 0), vertices=8):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=depth, location=loc, rotation=rot)
    o = bpy.context.object
    o.name = name
    assign(o, material)
    move_to(o, collection)
    return o


def sphere(name, loc, scale, material, collection, segments=8, rings=4):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=segments, ring_count=rings, location=loc)
    o = bpy.context.object
    o.name = name
    o.scale = scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    assign(o, material)
    move_to(o, collection)
    return o


def cone(name, loc, r1, r2, depth, material, collection, rot=(0, 0, 0), vertices=7):
    bpy.ops.mesh.primitive_cone_add(vertices=vertices, radius1=r1, radius2=r2, depth=depth, location=loc, rotation=rot)
    o = bpy.context.object
    o.name = name
    assign(o, material)
    move_to(o, collection)
    return o


def beam_between(name, start, end, radius, material, collection, vertices=7):
    a, b = Vector(start), Vector(end)
    direction = b - a
    o = cyl(name, (a + b) * 0.5, radius, direction.length, material, collection, vertices=vertices)
    o.rotation_mode = 'QUATERNION'
    o.rotation_quaternion = direction.to_track_quat('Z', 'Y')
    return o


def taper_between(name, start, end, radius_start, radius_end, material, collection, vertices=6):
    a, b = Vector(start), Vector(end)
    direction = b - a
    o = cone(name, (a + b) * 0.5, radius_start, radius_end, direction.length, material, collection, vertices=vertices)
    o.rotation_mode = 'QUATERNION'
    o.rotation_quaternion = direction.to_track_quat('Z', 'Y')
    return o


def make_armature(name, bones, collection, x=0):
    arm_data = bpy.data.armatures.new(name + '_Skeleton')
    arm = bpy.data.objects.new(name, arm_data)
    collection.objects.link(arm)
    arm.location.x = x
    arm.show_in_front = True
    bpy.context.view_layer.objects.active = arm
    arm.select_set(True)
    bpy.ops.object.mode_set(mode='EDIT')
    made = {}
    for bone_name, head, tail, parent in bones:
        b = arm.data.edit_bones.new(bone_name)
        b.head = head
        b.tail = tail
        if parent:
            b.parent = made[parent]
        made[bone_name] = b
    bpy.ops.object.mode_set(mode='POSE')
    for pb in arm.pose.bones:
        pb.rotation_mode = 'XYZ'
    bpy.ops.object.mode_set(mode='OBJECT')
    arm.select_set(False)
    return arm


def bind_rigid(obj, arm, bone):
    obj.parent = arm
    obj.parent_type = 'OBJECT'
    mod = obj.modifiers.new('SkeletalDeform', 'ARMATURE')
    mod.object = arm
    vg = obj.vertex_groups.new(name=bone)
    vg.add(range(len(obj.data.vertices)), 1.0, 'REPLACE')
    return obj


def detail_tag(obj, group):
    obj['detail_group'] = group
    obj['lod_recommendation'] = 'keep_lod0_remove_lod2'
    return obj


def hide_variant(obj):
    obj.hide_render = True
    obj.hide_set(True)
    obj['modular_variant'] = True


def action(arm, name, frames, loop=False, transition=None):
    act = bpy.data.actions.new(name)
    act.use_fake_user = True
    arm.animation_data_create()
    arm.animation_data.action = act
    for frame, poses in frames.items():
        for bone_name, values in poses.items():
            pb = arm.pose.bones.get(bone_name)
            if not pb:
                continue
            if 'rot' in values:
                pb.rotation_euler = [math.radians(v) for v in values['rot']]
                pb.keyframe_insert('rotation_euler', frame=frame, group=bone_name)
            if 'loc' in values:
                pb.location = values['loc']
                pb.keyframe_insert('location', frame=frame, group=bone_name)
    act['loop'] = loop
    act['fps'] = 30
    act['recommended_blend_frames'] = 5 if transition else 3
    if transition:
        act['transition'] = transition
    arm.animation_data.action = None
    for pb in arm.pose.bones:
        pb.rotation_euler = (0, 0, 0)
        pb.location = (0, 0, 0)
    return act


def humanoid_actions(arm):
    neutral = {1: {}}
    action(arm, 'HUM_Pose_Turn_Neg45', {1: {'spine': {'rot': (0, 0, 10)}, 'chest': {'rot': (0, 0, 18)}, 'head': {'rot': (0, 0, 17)}}})
    action(arm, 'HUM_Pose_Turn_Zero', neutral)
    action(arm, 'HUM_Pose_Turn_Pos45', {1: {'spine': {'rot': (0, 0, -10)}, 'chest': {'rot': (0, 0, -18)}, 'head': {'rot': (0, 0, -17)}}})
    walk = {
        1: {'upper_arm.L': {'rot': (28,0,0)}, 'upper_arm.R': {'rot': (-28,0,0)}, 'thigh.L': {'rot': (-30,0,0)}, 'thigh.R': {'rot': (30,0,0)}, 'root': {'loc': (0,0,0.02)}},
        8: {'upper_arm.L': {'rot': (0,0,0)}, 'upper_arm.R': {'rot': (0,0,0)}, 'thigh.L': {'rot': (0,0,0)}, 'thigh.R': {'rot': (0,0,0)}, 'root': {'loc': (0,0,0.0)}},
        15: {'upper_arm.L': {'rot': (-28,0,0)}, 'upper_arm.R': {'rot': (28,0,0)}, 'thigh.L': {'rot': (30,0,0)}, 'thigh.R': {'rot': (-30,0,0)}, 'root': {'loc': (0,0,0.02)}},
        22: {'upper_arm.L': {'rot': (0,0,0)}, 'upper_arm.R': {'rot': (0,0,0)}, 'thigh.L': {'rot': (0,0,0)}, 'thigh.R': {'rot': (0,0,0)}, 'root': {'loc': (0,0,0.0)}},
        29: {'upper_arm.L': {'rot': (28,0,0)}, 'upper_arm.R': {'rot': (-28,0,0)}, 'thigh.L': {'rot': (-30,0,0)}, 'thigh.R': {'rot': (30,0,0)}, 'root': {'loc': (0,0,0.02)}}}
    action(arm, 'HUM_Walk_Loop', walk, True)
    run = {
        1: {'chest': {'rot': (12,0,0)}, 'upper_arm.L': {'rot': (55,0,0)}, 'upper_arm.R': {'rot': (-55,0,0)}, 'thigh.L': {'rot': (-48,0,0)}, 'thigh.R': {'rot': (48,0,0)}, 'root': {'loc': (0,0,0.06)}},
        6: {'chest': {'rot': (12,0,0)}, 'upper_arm.L': {'rot': (0,0,0)}, 'upper_arm.R': {'rot': (0,0,0)}, 'thigh.L': {'rot': (0,0,0)}, 'thigh.R': {'rot': (0,0,0)}, 'root': {'loc': (0,0,0)}},
        11: {'chest': {'rot': (12,0,0)}, 'upper_arm.L': {'rot': (-55,0,0)}, 'upper_arm.R': {'rot': (55,0,0)}, 'thigh.L': {'rot': (48,0,0)}, 'thigh.R': {'rot': (-48,0,0)}, 'root': {'loc': (0,0,0.06)}},
        16: {'chest': {'rot': (12,0,0)}, 'upper_arm.L': {'rot': (0,0,0)}, 'upper_arm.R': {'rot': (0,0,0)}, 'thigh.L': {'rot': (0,0,0)}, 'thigh.R': {'rot': (0,0,0)}, 'root': {'loc': (0,0,0)}},
        21: {'chest': {'rot': (12,0,0)}, 'upper_arm.L': {'rot': (55,0,0)}, 'upper_arm.R': {'rot': (-55,0,0)}, 'thigh.L': {'rot': (-48,0,0)}, 'thigh.R': {'rot': (48,0,0)}, 'root': {'loc': (0,0,0.06)}}}
    action(arm, 'HUM_Run_Loop', run, True)
    action(arm, 'HUM_Turn_Left_90', {1:{}, 8:{'root':{'rot':(0,0,45)}, 'thigh.L':{'rot':(0,0,-12)}, 'thigh.R':{'rot':(0,0,12)}}, 16:{'root':{'rot':(0,0,90)}}})
    action(arm, 'HUM_Turn_Right_90', {1:{}, 8:{'root':{'rot':(0,0,-45)}, 'thigh.L':{'rot':(0,0,12)}, 'thigh.R':{'rot':(0,0,-12)}}, 16:{'root':{'rot':(0,0,-90)}}})
    action(arm, 'HUM_Attack_Melee', {1:{}, 8:{'chest':{'rot':(0,0,-25)},'upper_arm.R':{'rot':(-80,15,-25)},'lower_arm.R':{'rot':(-45,0,0)}}, 13:{'chest':{'rot':(0,0,35)},'upper_arm.R':{'rot':(70,-10,35)},'lower_arm.R':{'rot':(-15,0,0)}}, 24:{}})
    action(arm, 'HUM_Jump', {1:{'root':{'loc':(0,0,0)},'thigh.L':{'rot':(-15,0,0)},'thigh.R':{'rot':(-15,0,0)}}, 8:{'root':{'loc':(0,0,0.55)},'thigh.L':{'rot':(22,0,0)},'thigh.R':{'rot':(22,0,0)}}, 16:{'root':{'loc':(0,0,0.95)}}, 24:{'root':{'loc':(0,0,0.45)},'thigh.L':{'rot':(-25,0,0)},'thigh.R':{'rot':(-25,0,0)}}, 30:{'root':{'loc':(0,0,0)}}})
    action(arm, 'HUM_Walk_To_Run', {1:{'chest':{'rot':(0,0,0)}}, 10:{'chest':{'rot':(12,0,0)},'upper_arm.L':{'rot':(35,0,0)},'upper_arm.R':{'rot':(-35,0,0)}}, 16:{'chest':{'rot':(12,0,0)},'upper_arm.L':{'rot':(55,0,0)},'upper_arm.R':{'rot':(-55,0,0)}}}, transition='HUM_Walk_Loop -> HUM_Run_Loop')
    action(arm, 'HUM_Run_To_Walk', {1:{'chest':{'rot':(12,0,0)},'upper_arm.L':{'rot':(55,0,0)},'upper_arm.R':{'rot':(-55,0,0)}}, 10:{'chest':{'rot':(5,0,0)},'upper_arm.L':{'rot':(35,0,0)},'upper_arm.R':{'rot':(-35,0,0)}}, 16:{'chest':{'rot':(0,0,0)},'upper_arm.L':{'rot':(28,0,0)},'upper_arm.R':{'rot':(-28,0,0)}}}, transition='HUM_Run_Loop -> HUM_Walk_Loop')


def build_humanoid(root):
    c = coll('CHAR_Humanoid_Modular', root)
    rigc = coll('HUM_Rig', c)
    basec = coll('HUM_BaseBody', c)
    headc = coll('HUM_Heads_Faces', c)
    clothc = coll('HUM_Clothing_Armor', c)
    equipc = coll('HUM_Equipment', c)
    bones = [
        ('root',(0,0,0),(0,0,0.16),None),('pelvis',(0,0,0.82),(0,0,1.02),'root'),('spine',(0,0,1.02),(0,0,1.25),'pelvis'),('chest',(0,0,1.25),(0,0,1.48),'spine'),('neck',(0,0,1.48),(0,0,1.57),'chest'),('head',(0,0,1.57),(0,0,1.82),'neck'),
        ('upper_arm.L',(0,0,1.43),(0.34,0,1.25),'chest'),('lower_arm.L',(0.34,0,1.25),(0.58,0,1.06),'upper_arm.L'),('hand.L',(0.58,0,1.06),(0.67,0,0.99),'lower_arm.L'),
        ('upper_arm.R',(0,0,1.43),(-0.34,0,1.25),'chest'),('lower_arm.R',(-0.34,0,1.25),(-0.58,0,1.06),'upper_arm.R'),('hand.R',(-0.58,0,1.06),(-0.67,0,0.99),'lower_arm.R'),
        ('thigh.L',(0.13,0,0.88),(0.13,0,0.49),'pelvis'),('shin.L',(0.13,0,0.49),(0.13,0,0.12),'thigh.L'),('foot.L',(0.13,0,0.12),(0.13,-0.18,0.05),'shin.L'),
        ('thigh.R',(-0.13,0,0.88),(-0.13,0,0.49),'pelvis'),('shin.R',(-0.13,0,0.49),(-0.13,0,0.12),'thigh.R'),('foot.R',(-0.13,0,0.12),(-0.13,-0.18,0.05),'shin.R')]
    arm = make_armature('RIG_Humanoid_7Head', bones, rigc, x=-3.0)
    arm['height_m'] = 1.82
    arm['forward_axis'] = '-Y'
    arm['modular_standard'] = 'HUM_v1'
    pieces = [
        (cube('HUM_Body_Pelvis', (0,0,0.92),(0.22,0.13,0.13),M['cloth'],basec), 'pelvis'),
        (cube('HUM_Body_Torso', (0,0,1.29),(0.27,0.14,0.25),M['cloth'],basec), 'chest'),
        (sphere('HUM_Head_Base_A',(0,0,1.68),(0.14,0.12,0.17),M['skin_pale'],headc), 'head'),
    ]
    for side, sx in [('L',1),('R',-1)]:
        pieces += [
            (cyl(f'HUM_Body_UpperArm_{side}',(0.29*sx,0,1.29),0.075,0.38,M['skin_pale'],basec,rot=(0,math.radians(90),math.radians(32*sx))),f'upper_arm.{side}'),
            (cyl(f'HUM_Body_ForeArm_{side}',(0.50*sx,0,1.13),0.065,0.31,M['skin_pale'],basec,rot=(0,math.radians(90),math.radians(39*sx))),f'lower_arm.{side}'),
            (sphere(f'HUM_Body_Hand_{side}',(0.64*sx,-0.005,1.01),(0.07,0.055,0.09),M['skin_pale'],basec),f'hand.{side}'),
            (cyl(f'HUM_Body_Thigh_{side}',(0.13*sx,0,0.69),0.105,0.40,M['cloth'],basec),f'thigh.{side}'),
            (cyl(f'HUM_Body_Shin_{side}',(0.13*sx,0,0.30),0.085,0.38,M['cloth'],basec),f'shin.{side}'),
            (cube(f'HUM_Body_Foot_{side}',(0.13*sx,-0.08,0.08),(0.09,0.16,0.07),M['armor'],basec),f'foot.{side}')]
    for obj,bone in pieces:
        bind_rigid(obj,arm,bone)
    # Alternate modular components, aligned to the same skeleton.
    alt_head = sphere('HUM_Head_Base_B_Angular',(0,0,1.68),(0.13,0.115,0.18),M['skin_dark'],headc,segments=6,rings=4); bind_rigid(alt_head,arm,'head'); hide_variant(alt_head)
    face = cube('HUM_Face_Visor_Module',(0,-0.117,1.69),(0.105,0.018,0.045),M['cyan'],headc,0.01); bind_rigid(face,arm,'head')
    mask = cube('HUM_Face_Respirator_Module',(0,-0.148,1.61),(0.095,0.040,0.060),M['armor'],headc,0.018); bind_rigid(mask,arm,'head')
    helmet = sphere('HUM_Head_Helmet_Assault',(0,0.018,1.70),(0.175,0.150,0.205),M['armor'],headc,segments=10,rings=5); bind_rigid(helmet,arm,'head')
    helmet_band = cube('HUM_Helmet_BrowBand',(0,-0.132,1.75),(0.15,0.025,0.035),M['steel'],headc,0.012); bind_rigid(helmet_band,arm,'head')
    helmet_ridge = cube('HUM_Helmet_TopRidge',(0,0.005,1.895),(0.040,0.10,0.035),M['armor2'],headc,0.014); bind_rigid(helmet_ridge,arm,'head')
    for side,sx in [('L',1),('R',-1)]:
        pod=cyl(f'HUM_Helmet_SidePod_{side}',(0.174*sx,0.0,1.70),0.052,0.055,M['armor2'],headc,rot=(0,math.radians(90),0),vertices=8);bind_rigid(pod,arm,'head')
        lamp=sphere(f'HUM_Helmet_SideLamp_{side}',(0.203*sx,-0.020,1.735),(0.020,0.018,0.020),M['red'],headc,segments=6,rings=3);bind_rigid(lamp,arm,'head')
    chest = cube('HUM_Armor_Chest_Scavenger',(0,-0.10,1.34),(0.30,0.08,0.23),M['armor2'],clothc,0.04); bind_rigid(chest,arm,'chest')
    chest2 = cube('HUM_Armor_Chest_Military',(0,-0.11,1.34),(0.32,0.075,0.25),M['armor'],clothc,0.025); bind_rigid(chest2,arm,'chest'); hide_variant(chest2)
    backpack = cube('HUM_Equipment_LifeSupportPack',(0,0.19,1.32),(0.19,0.08,0.22),M['wall'],equipc,0.03); bind_rigid(backpack,arm,'chest')
    weapon = cube('HUM_Equipment_PulseRifle',(-0.68,-0.08,1.00),(0.07,0.34,0.08),M['armor'],equipc,0.025); bind_rigid(weapon,arm,'hand.R'); hide_variant(weapon)
    # Layered detail pass: readable at quarter-view while retaining a low-poly silhouette.
    detail_parts=[]
    detail_parts += [(cube('HUM_Armor_ChestCenter',(0,-0.194,1.36),(0.12,0.018,0.16),M['armor'],clothc,0.012),'chest'),
                     (cube('HUM_Armor_ChestCollar',(0,-0.182,1.51),(0.22,0.025,0.035),M['steel'],clothc,0.012),'chest'),
                     (cube('HUM_Gear_Belt',(0,-0.145,1.02),(0.235,0.03,0.045),M['armor2'],equipc,0.012),'pelvis'),
                     (cube('HUM_Gear_BeltBuckle',(0,-0.181,1.02),(0.045,0.012,0.035),M['amber'],equipc,0.006),'pelvis'),
                     (cube('HUM_Face_Chin',(0,-0.123,1.59),(0.07,0.025,0.035),M['skin_pale'],headc,0.01),'head'),
                     (cone('HUM_Face_Nose',(0,-0.139,1.68),0.025,0.007,0.065,M['skin_pale'],headc,rot=(math.radians(90),0,0),vertices=5),'head')]
    for side,sx in [('L',1),('R',-1)]:
        detail_parts += [(cube(f'HUM_Armor_Shoulder_{side}',(0.25*sx,-0.025,1.43),(0.115,0.15,0.075),M['armor'],clothc,0.035,rot=(0,math.radians(8*sx),math.radians(8*sx))),f'upper_arm.{side}'),
                         (cube(f'HUM_Armor_Forearm_{side}',(0.50*sx,-0.035,1.14),(0.10,0.085,0.12),M['armor2'],clothc,0.025,rot=(0,0,math.radians(38*sx))),f'lower_arm.{side}'),
                         (cube(f'HUM_Armor_Knee_{side}',(0.13*sx,-0.085,0.49),(0.09,0.045,0.075),M['armor'],clothc,0.022),f'shin.{side}'),
                         (cube(f'HUM_Boot_Ankle_{side}',(0.13*sx,0,0.16),(0.105,0.11,0.09),M['black'],clothc,0.022),f'shin.{side}'),
                         (cube(f'HUM_Belt_Pouch_{side}',(0.18*sx,-0.155,1.00),(0.055,0.035,0.07),M['cloth'],equipc,0.015),'pelvis')]
        detail_parts += [(cube(f'HUM_Armor_ThighPlate_{side}',(0.13*sx,-0.085,0.72),(0.095,0.040,0.15),M['armor'],clothc,0.020),f'thigh.{side}'),
                         (cube(f'HUM_Armor_HipPlate_{side}',(0.23*sx,-0.030,0.94),(0.07,0.11,0.10),M['armor2'],clothc,0.020),'pelvis'),
                         (cyl(f'HUM_Suit_HosePort_{side}',(0.17*sx,-0.165,1.22),0.025,0.045,M['cyan'],equipc,rot=(math.radians(90),0,0),vertices=8),'chest')]
    for i,z in enumerate([1.11,1.17,1.23]):
        detail_parts.append((cube(f'HUM_Armor_AbdomenPlate_{i}',(0,-0.158,z),(0.16-i*0.012,0.018,0.028),M['steel'] if i==1 else M['armor'],clothc,0.008),'spine'))
    # Backpack vents and status indicators.
    for i,z in enumerate([1.20,1.30,1.40]):
        detail_parts.append((cube(f'HUM_LifeSupport_Vent_{i}',(0,0.281,z),(0.11,0.012,0.018),M['black'],equipc,0.004),'chest'))
    detail_parts.append((sphere('HUM_LifeSupport_Status',(0.13,0.281,1.47),(0.018,0.012,0.018),M['cyan'],equipc,segments=6,rings=3),'chest'))
    for obj,bone in detail_parts:
        bind_rigid(detail_tag(obj,'HUM_Lod0Detail'),arm,bone)
    for obj in c.all_objects:
        if obj.type == 'MESH': obj['socket_standard'] = 'HUM_v1'
    humanoid_actions(arm)
    return c, arm


def void_actions(arm):
    action(arm,'VOID_Pose_Turn_Neg45',{1:{'spine':{'rot':(0,0,12)},'neck':{'rot':(0,0,18)},'head':{'rot':(0,0,15)}}})
    action(arm,'VOID_Pose_Turn_Zero',{1:{}})
    action(arm,'VOID_Pose_Turn_Pos45',{1:{'spine':{'rot':(0,0,-12)},'neck':{'rot':(0,0,-18)},'head':{'rot':(0,0,-15)}}})
    walk={1:{'leg_front.L':{'rot':(25,0,0)},'leg_front.R':{'rot':(-25,0,0)},'leg_back.L':{'rot':(-22,0,0)},'leg_back.R':{'rot':(22,0,0)}},9:{'leg_front.L':{'rot':(-25,0,0)},'leg_front.R':{'rot':(25,0,0)},'leg_back.L':{'rot':(22,0,0)},'leg_back.R':{'rot':(-22,0,0)}},17:{'leg_front.L':{'rot':(25,0,0)},'leg_front.R':{'rot':(-25,0,0)},'leg_back.L':{'rot':(-22,0,0)},'leg_back.R':{'rot':(22,0,0)}}}
    action(arm,'VOID_Walk_Loop',walk,True)
    run={1:{'root':{'loc':(0,0,0.08)},'spine':{'rot':(12,0,0)},'leg_front.L':{'rot':(42,0,0)},'leg_front.R':{'rot':(-42,0,0)},'leg_back.L':{'rot':(-38,0,0)},'leg_back.R':{'rot':(38,0,0)}},6:{'root':{'loc':(0,0,0)},'leg_front.L':{'rot':(-42,0,0)},'leg_front.R':{'rot':(42,0,0)},'leg_back.L':{'rot':(38,0,0)},'leg_back.R':{'rot':(-38,0,0)}},11:{'root':{'loc':(0,0,0.08)},'spine':{'rot':(12,0,0)},'leg_front.L':{'rot':(42,0,0)},'leg_front.R':{'rot':(-42,0,0)},'leg_back.L':{'rot':(-38,0,0)},'leg_back.R':{'rot':(38,0,0)}}}
    action(arm,'VOID_Run_Loop',run,True)
    action(arm,'VOID_Turn_Left_90',{1:{},8:{'root':{'rot':(0,0,45)},'neck':{'rot':(0,0,-20)}},16:{'root':{'rot':(0,0,90)}}})
    action(arm,'VOID_Turn_Right_90',{1:{},8:{'root':{'rot':(0,0,-45)},'neck':{'rot':(0,0,20)}},16:{'root':{'rot':(0,0,-90)}}})
    action(arm,'VOID_Attack_Pounce',{1:{'spine':{'rot':(-8,0,0)}},8:{'root':{'loc':(0,-0.18,0.12)},'spine':{'rot':(25,0,0)},'blade.L':{'rot':(-60,0,20)},'blade.R':{'rot':(-60,0,-20)}},13:{'root':{'loc':(0,-0.65,0.30)},'head':{'rot':(-25,0,0)},'blade.L':{'rot':(35,0,-15)},'blade.R':{'rot':(35,0,15)}},24:{}})
    action(arm,'VOID_Jump',{1:{'root':{'loc':(0,0,0)},'leg_back.L':{'rot':(-25,0,0)},'leg_back.R':{'rot':(-25,0,0)}},8:{'root':{'loc':(0,-0.22,0.45)},'leg_back.L':{'rot':(30,0,0)},'leg_back.R':{'rot':(30,0,0)}},15:{'root':{'loc':(0,-0.5,0.85)}},23:{'root':{'loc':(0,-0.75,0.35)}},30:{'root':{'loc':(0,-0.85,0)}}})
    action(arm,'VOID_Walk_To_Run',{1:{'spine':{'rot':(0,0,0)}},12:{'spine':{'rot':(12,0,0)},'root':{'loc':(0,0,0.06)}}},transition='VOID_Walk_Loop -> VOID_Run_Loop')
    action(arm,'VOID_Run_To_Walk',{1:{'spine':{'rot':(12,0,0)},'root':{'loc':(0,0,0.06)}},12:{'spine':{'rot':(0,0,0)},'root':{'loc':(0,0,0)}}},transition='VOID_Run_Loop -> VOID_Walk_Loop')


def build_void(root):
    c=coll('CHAR_Void_Quadruped_Modular',root); rigc=coll('VOID_Rig',c); basec=coll('VOID_BaseBody',c); modc=coll('VOID_ModularParts',c)
    bones=[('root',(0,0,0.25),(0,0,0.45),None),('spine',(0,0,0.48),(0,-0.50,0.62),'root'),('chest',(0,-0.50,0.62),(0,-0.88,0.68),'spine'),('neck',(0,-0.88,0.68),(0,-1.12,0.66),'chest'),('head',(0,-1.12,0.66),(0,-1.42,0.59),'neck'),('tail',(0,0.15,0.48),(0,0.75,0.52),'spine'),
    ('leg_front.L',(0.23,-0.62,0.58),(0.35,-0.70,0.28),'chest'),('paw_front.L',(0.35,-0.70,0.28),(0.37,-0.91,0.06),'leg_front.L'),('leg_front.R',(-0.23,-0.62,0.58),(-0.35,-0.70,0.28),'chest'),('paw_front.R',(-0.35,-0.70,0.28),(-0.37,-0.91,0.06),'leg_front.R'),
    ('leg_back.L',(0.25,-0.02,0.48),(0.44,0.08,0.25),'spine'),('paw_back.L',(0.44,0.08,0.25),(0.42,-0.05,0.05),'leg_back.L'),('leg_back.R',(-0.25,-0.02,0.48),(-0.44,0.08,0.25),'spine'),('paw_back.R',(-0.44,0.08,0.25),(-0.42,-0.05,0.05),'leg_back.R'),
    ('blade.L',(0.22,-0.82,0.68),(0.52,-1.08,0.72),'chest'),('blade.R',(-0.22,-0.82,0.68),(-0.52,-1.08,0.72),'chest')]
    arm=make_armature('RIG_Void_Quadruped',bones,rigc,x=0.0); arm['forward_axis']='-Y'; arm['modular_standard']='VOID_v1'
    parts=[(sphere('VOID_Body_Abdomen',(0,-0.05,0.50),(0.34,0.48,0.25),M['flesh'],basec),'spine'),(sphere('VOID_Body_Thorax',(0,-0.62,0.64),(0.38,0.42,0.29),M['void'],basec),'chest'),(sphere('VOID_Head_Base',(0,-1.20,0.63),(0.28,0.34,0.22),M['void'],basec,segments=7,rings=4),'head')]
    for side,sx in [('L',1),('R',-1)]:
        parts += [(cyl(f'VOID_Leg_Front_{side}',(0.29*sx,-0.67,0.42),0.09,0.40,M['flesh'],basec,rot=(math.radians(18),math.radians(18*sx),0),vertices=6),f'leg_front.{side}'),(cone(f'VOID_Paw_Front_{side}',(0.36*sx,-0.82,0.16),0.075,0.035,0.35,M['bone'],basec,rot=(math.radians(35),0,0),vertices=5),f'paw_front.{side}'),(cyl(f'VOID_Leg_Back_{side}',(0.34*sx,0.02,0.35),0.11,0.40,M['flesh'],basec,rot=(0,math.radians(35*sx),0),vertices=6),f'leg_back.{side}'),(cone(f'VOID_Paw_Back_{side}',(0.43*sx,0.00,0.14),0.08,0.035,0.32,M['bone'],basec,rot=(math.radians(20),0,0),vertices=5),f'paw_back.{side}'),(cone(f'VOID_Blade_Fore_{side}',(0.43*sx,-1.0,0.70),0.11,0.015,0.58,M['bone'],basec,rot=(math.radians(70),math.radians(18*sx),0),vertices=5),f'blade.{side}')]
    parts.append((cone('VOID_Tail_Base',(0,0.48,0.50),0.14,0.025,0.85,M['flesh'],basec,rot=(math.radians(90),0,0),vertices=7),'tail'))
    for obj,bone in parts: bind_rigid(obj,arm,bone)
    # Oversized dorsal scythes define the assault silhouette. Three rigid segments share
    # the blade bone so the existing pounce/attack action drives the complete claw.
    for side,sx in [('L',1),('R',-1)]:
        scythe_parts=[
            sphere(f'VOID_Scythe_Shoulder_{side}',(0.27*sx,-0.58,0.76),(0.13,0.12,0.13),M['void'],modc,segments=7,rings=4),
            beam_between(f'VOID_Scythe_UpperArm_{side}',(0.27*sx,-0.58,0.76),(0.39*sx,-0.43,1.08),0.085,M['flesh'],modc,vertices=7),
            taper_between(f'VOID_Scythe_BladeRoot_{side}',(0.39*sx,-0.43,1.08),(0.49*sx,-0.72,1.27),0.105,0.060,M['bone'],modc,vertices=6),
            taper_between(f'VOID_Scythe_BladeTip_{side}',(0.49*sx,-0.72,1.27),(0.46*sx,-1.38,0.91),0.078,0.006,M['bone'],modc,vertices=6)]
        for part in scythe_parts:
            bind_rigid(detail_tag(part,'VOID_PrimaryScythe'),arm,f'blade.{side}')
    horns=[]
    for side,sx in [('L',1),('R',-1)]:
        h=cone(f'VOID_Head_Horn_{side}',(0.19*sx,-1.34,0.80),0.07,0.0,0.38,M['bone'],modc,rot=(math.radians(55),math.radians(15*sx),0),vertices=5);bind_rigid(h,arm,'head');horns.append(h)
    for i,(ex,ez) in enumerate([(-0.09,0.71),(-0.035,0.74),(0.035,0.74),(0.09,0.71)]):
        eye=sphere(f'VOID_Face_Eye_{i}',(ex,-1.455,ez),(0.026,0.022,0.026),M['red'],modc,segments=6,rings=3);bind_rigid(eye,arm,'head')
    mouth=cube('VOID_Face_Mouth',(0,-1.465,0.58),(0.12,0.018,0.038),M['black'],modc,0.012);bind_rigid(mouth,arm,'head')
    for side,sx in [('L',1),('R',-1)]:
        mand=cone(f'VOID_Face_Mandible_{side}',(0.12*sx,-1.50,0.56),0.045,0.008,0.28,M['bone'],modc,rot=(math.radians(70),math.radians(22*sx),0),vertices=5);bind_rigid(mand,arm,'head')
        for idx,(px,py,pz,bone) in enumerate([(0.37,-0.93,0.055,f'paw_front.{side}'),(0.31,-0.91,0.05,f'paw_front.{side}'),(0.43,-0.18,0.045,f'paw_back.{side}'),(0.37,-0.16,0.04,f'paw_back.{side}')]):
            claw=cone(f'VOID_Claw_{side}_{idx}',(px*sx,py,pz),0.028,0.004,0.17,M['bone'],modc,rot=(math.radians(78),0,0),vertices=4);bind_rigid(claw,arm,bone);detail_tag(claw,'VOID_Lod0Detail')
    for i,(y,z,s) in enumerate([(0.18,0.75,0.20),(-0.10,0.79,0.23),(-0.38,0.82,0.21),(-0.66,0.84,0.17)]):
        plate=cone(f'VOID_Back_DorsalPlate_{i}',(0,y,z),s,0.015,s*1.45,M['void'],modc,rot=(0,0,0),vertices=5);bind_rigid(plate,arm,'spine' if i<2 else 'chest');detail_tag(plate,'VOID_Lod0Detail')
    for side,sx in [('L',1),('R',-1)]:
        joint=sphere(f'VOID_FrontJoint_{side}',(0.29*sx,-0.68,0.38),(0.115,0.105,0.105),M['void'],modc,segments=6,rings=3);bind_rigid(joint,arm,f'leg_front.{side}')
        joint2=sphere(f'VOID_BackJoint_{side}',(0.36*sx,0.03,0.30),(0.13,0.12,0.115),M['void'],modc,segments=6,rings=3);bind_rigid(joint2,arm,f'leg_back.{side}')
    shell=cube('VOID_Back_Carapace_Heavy',(0,0.0,0.75),(0.30,0.40,0.10),M['void'],modc,0.05);bind_rigid(shell,arm,'spine');hide_variant(shell)
    tailblade=cone('VOID_Tail_TerminalBlade',(0,0.89,0.51),0.12,0.0,0.42,M['bone'],modc,rot=(math.radians(90),0,0),vertices=5);bind_rigid(tailblade,arm,'tail')
    for obj in c.all_objects:
        if obj.type=='MESH':obj['socket_standard']='VOID_v1'
    void_actions(arm)
    return c,arm


def build_ship(root):
    c=coll('ENV_Ship_ModularKit',root); walls=coll('SHIP_Walls',c); devices=coll('SHIP_Devices',c); medical=coll('SHIP_Medical',c); controls=coll('SHIP_Controls',c); floors=coll('SHIP_Floors',c)
    # 2 m grid modular wall kit.
    cube('SHIP_Wall_Solid_2m',(3.6,0,1.0),(1.0,0.10,1.0),M['wall'],walls,0.025)
    cube('SHIP_Wall_Ribbed_2m',(5.8,0,1.0),(1.0,0.10,1.0),M['wall'],walls,0.025)
    for x in [5.0,5.4,5.8,6.2,6.6]: cube(f'SHIP_Wall_Rib_{x:.1f}',(x,-0.14,1.0),(0.045,0.05,0.90),M['trim'],walls,0.015)
    cube('SHIP_Wall_Corner_Inner',(7.4,0,1.0),(0.10,1.0,1.0),M['wall'],walls,0.025)
    cube('SHIP_Door_Frame_Top',(9.0,0,1.88),(1.0,0.14,0.12),M['trim'],walls,0.025)
    cube('SHIP_Door_Frame_L',(8.12,0,0.94),(0.12,0.14,0.94),M['trim'],walls,0.025)
    cube('SHIP_Door_Frame_R',(9.88,0,0.94),(0.12,0.14,0.94),M['trim'],walls,0.025)
    # Consoles and ship devices.
    cube('SHIP_Console_Base',(3.7,-2.2,0.48),(0.48,0.34,0.48),M['wall'],controls,0.06)
    cube('SHIP_Console_Screen',(3.7,-2.52,0.90),(0.42,0.04,0.24),M['cyan'],controls,0.025,rot=(math.radians(18),0,0))
    cube('SHIP_PilotSeat_Base',(5.1,-2.1,0.24),(0.34,0.38,0.24),M['armor'],controls,0.08)
    cube('SHIP_PilotSeat_Back',(5.1,-1.78,0.72),(0.34,0.12,0.56),M['armor'],controls,0.08,rot=(math.radians(-8),0,0))
    cyl('SHIP_Device_OxygenTank',(6.5,-2.0,0.68),0.23,1.25,M['medical'],devices,vertices=10)
    cube('SHIP_Device_PowerJunction',(7.4,-2.0,0.62),(0.42,0.25,0.62),M['wall'],devices,0.05)
    for z in [0.35,0.62,0.89]: cube(f'SHIP_Device_PowerLight_{z}',(7.4,-2.27,z),(0.22,0.025,0.035),M['amber'],devices,0.01)
    # Medical station and pod.
    cube('SHIP_MedBed_Base',(8.9,-2.0,0.42),(0.92,0.38,0.22),M['medical'],medical,0.08)
    cube('SHIP_MedBed_Pad',(8.9,-2.0,0.67),(0.82,0.33,0.08),M['cloth'],medical,0.04)
    cube('SHIP_MedScanner_Arch',(8.9,-2.0,1.20),(0.08,0.52,0.08),M['cyan'],medical,0.03)
    cube('SHIP_MedCabinet',(10.7,-2.0,0.82),(0.55,0.25,0.82),M['medical'],medical,0.05)
    # Architectural seams, fasteners, vents and controls break up large silhouettes.
    for wall_x in [3.6,5.8]:
        cube(f'SHIP_Wall_Inset_{wall_x}',(wall_x,-0.112,1.0),(0.72,0.012,0.70),M['black'],walls,0.018)
        for sx in [-0.82,0.82]:
            for z in [0.22,1.78]:
                cyl(f'SHIP_Wall_Bolt_{wall_x}_{sx}_{z}',(wall_x+sx,-0.135,z),0.035,0.025,M['steel'],walls,rot=(math.radians(90),0,0),vertices=6)
    for i,z in enumerate([0.48,0.62,0.76]):
        cube(f'SHIP_Console_Key_{i}',(3.70+(i-1)*0.12,-2.574,z),(0.042,0.012,0.025),M['amber'] if i==1 else M['cyan'],controls,0.006)
    for i,x in enumerate([4.87,5.10,5.33]):
        cyl(f'SHIP_PilotSeat_BaseRail_{i}',(x,-2.18,0.08),0.035,0.65,M['steel'],controls,rot=(math.radians(90),0,0),vertices=8)
    cube('SHIP_MedBed_SideRail_L',(8.9-0.82,-2.02,0.82),(0.035,0.34,0.14),M['steel'],medical,0.018)
    cube('SHIP_MedBed_SideRail_R',(8.9+0.82,-2.02,0.82),(0.035,0.34,0.14),M['steel'],medical,0.018)
    cube('SHIP_MedBed_Monitor',(9.62,-2.18,1.18),(0.18,0.045,0.15),M['cyan'],medical,0.025,rot=(0,math.radians(-12),0))
    for i,z in enumerate([0.30,0.55,0.80,1.05,1.30]):
        cube(f'SHIP_MedCabinet_Drawer_{i}',(10.7,-2.268,z),(0.45,0.018,0.085),M['trim'],medical,0.012)
        cube(f'SHIP_MedCabinet_Handle_{i}',(10.7,-2.295,z),(0.10,0.010,0.018),M['steel'],medical,0.006)
    for i,z in enumerate([0.32,0.48,0.64,0.80,0.96]):
        cube(f'SHIP_PowerJunction_Vent_{i}',(7.4+(i%2)*0.08-0.04,-2.272,z),(0.13,0.012,0.018),M['black'],devices,0.004)
    # Door access panel and hazard marker.
    cube('SHIP_Door_AccessPanel',(9.66,-0.16,1.08),(0.10,0.035,0.20),M['black'],walls,0.018)
    cube('SHIP_Door_AccessLight',(9.66,-0.198,1.15),(0.055,0.012,0.025),M['cyan'],walls,0.006)
    # Floor tiles are material samples and snap-ready geometry.
    floor_mats=[]
    for name,color,metal,rough in [('MAT_Floor_Deck',(0.045,0.055,0.060),0.85,0.42),('MAT_Floor_Caution',(0.25,0.12,0.012),0.55,0.5),('MAT_Floor_Dust',(0.12,0.075,0.04),0.15,0.9),('MAT_Floor_BioCorruption',(0.12,0.025,0.07),0.05,0.68)]: floor_mats.append(mat(name,color,metal,rough))
    for i,fm in enumerate(floor_mats):
        tile=cube(f'SHIP_FloorTile_{fm.name.replace("MAT_Floor_","")}',(3.6+i*2.2,-4.0,0),(1.0,1.0,0.06),fm,floors,0.025)
        for j in [-0.65,0,0.65]: cube(f'{tile.name}_Groove_{j}',(tile.location.x+j,-4.0,-0.07),(0.025,1.0,0.015),M['trim'],floors,0.005)
        for j in [-0.65,0,0.65]: cube(f'{tile.name}_CrossGroove_{j}',(tile.location.x,-4.0+j,-0.068),(1.0,0.022,0.014),M['black'],floors,0.004)
    # Raised grate and warning stripes as reusable overlays.
    for i in range(7):
        cube(f'SHIP_Floor_GrateBar_{i}',(3.6-0.72+i*0.24,-4.0,0.075),(0.065,0.72,0.018),M['steel'],floors,0.01)
    for i in range(6):
        cube(f'SHIP_Floor_HazardStripe_{i}',(5.8-0.82+i*0.32,-4.0,0.078),(0.11,0.80,0.016),M['amber'] if i%2==0 else M['black'],floors,0.006,rot=(0,0,math.radians(12)))
    for obj in c.all_objects:
        if obj.type=='MESH': obj['snap_grid_m']=0.25
    return c


def asteroid(name, loc, scale, seed, collection):
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2, radius=1.0, location=loc)
    o=bpy.context.object;o.name=name
    for v in o.data.vertices:
        p=v.co.normalized(); n=math.sin(p.x*7.1+seed)+math.sin(p.y*9.7-seed*0.4)+math.sin(p.z*11.3+seed*0.7)
        v.co *= 1.0+n*0.075
    o.scale=scale;bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);assign(o,M['rock']);move_to(o,collection)
    o['collision_hint']='convex_hull';o['lod_hint']='decimate_0.5'
    return o


def build_space(root):
    c=coll('ENV_Space_Asteroids',root)
    for i,(loc,sc) in enumerate([((12,-1,0.7),(0.75,0.55,0.62)),((14,-1.3,1.0),(1.15,0.72,0.90)),((16.5,-0.6,0.55),(0.48,0.40,0.52)),((18,-1.5,1.15),(1.4,1.0,0.78))]):
        o=asteroid(f'SPACE_Asteroid_{chr(65+i)}',loc,sc,2.3+i*1.7,c)
        o.rotation_euler=(0.2*i,0.35*i,0.12*i)
        o.keyframe_insert('rotation_euler',frame=1)
        o.rotation_euler=(0.2*i+0.6,0.35*i+1.4,0.12*i+2.0)
        o.keyframe_insert('rotation_euler',frame=121)
        if o.animation_data and o.animation_data.action:
            o.animation_data.action.name=f'SPACE_Asteroid_{chr(65+i)}_DriftSpin'
            o.animation_data.action.use_fake_user=True
        # Separate surface fragments preserve a cheap low-poly cratered read without booleans.
        for j in range(3):
            ang=(i*1.7+j*2.05)
            frag=sphere(f'SPACE_Asteroid_{chr(65+i)}_SurfaceChunk_{j}',(loc[0]+math.cos(ang)*sc[0]*0.72,loc[1]+math.sin(ang)*sc[1]*0.72,loc[2]+(j-1)*sc[2]*0.26),(0.16*sc[0],0.10*sc[1],0.12*sc[2]),M['dust'],c,segments=6,rings=3)
            frag.rotation_euler=(ang*0.3,ang*0.2,ang)
            frag.parent=o
            frag.matrix_parent_inverse=o.matrix_world.inverted()
    return c


def setup_scene():
    sc=bpy.context.scene
    sc.render.engine='BLENDER_EEVEE'
    sc.render.resolution_x=1280;sc.render.resolution_y=720;sc.render.resolution_percentage=100
    sc.render.image_settings.file_format='PNG'
    sc.render.film_transparent=False
    sc.world.color=(0.004,0.006,0.012)
    sc.frame_start=1;sc.frame_end=120;sc.render.fps=30
    sc['asset_pack']='DarkSciFi_Modular_v1_1'
    sc['unit_scale_m']=1.0
    sc.unit_settings.system='METRIC'


def camera_and_light():
    bpy.ops.object.camera_add(location=(8,-29,17),rotation=(math.radians(63),0,math.radians(34)))
    cam=bpy.context.object;cam.name='PREVIEW_Camera';bpy.context.scene.camera=cam
    # Aim camera at overall showcase center.
    direction=Vector((7,-1.5,0.8))-cam.location
    cam.rotation_euler=direction.to_track_quat('-Z','Y').to_euler();cam.data.lens=52
    bpy.ops.object.light_add(type='AREA',location=(2,-6,9));key=bpy.context.object;key.name='PREVIEW_Key';key.data.energy=1800;key.data.shape='DISK';key.data.size=7;key.data.color=(0.48,0.68,1.0)
    bpy.ops.object.light_add(type='AREA',location=(11,2,5));fill=bpy.context.object;fill.name='PREVIEW_Amber';fill.data.energy=1100;fill.data.size=5;fill.data.color=(1.0,0.28,0.08)
    bpy.ops.object.light_add(type='AREA',location=(-5,-1,3));rim=bpy.context.object;rim.name='PREVIEW_Rim';rim.data.energy=900;rim.data.size=4;rim.data.color=(0.18,0.75,1.0)


def save_libraries(root, category_collections):
    master=os.path.join(ROOT,'DarkSciFi_ModularAssetPack.blend')
    bpy.ops.wm.save_as_mainfile(filepath=master,compress=True)
    action_prefix={'Humanoid_Modular.blend':'HUM_','Void_Quadruped.blend':'VOID_','Space_Asteroids.blend':'SPACE_'}
    for filename,collections in category_collections.items():
        blocks=set(collections)
        prefix=action_prefix.get(filename)
        if prefix:
            blocks.update(a for a in bpy.data.actions if a.name.startswith(prefix))
        bpy.data.libraries.write(os.path.join(LIB,filename),blocks,fake_user=True,compress=True)


def aim_camera(target, location, lens=52):
    cam=bpy.data.objects['PREVIEW_Camera']
    cam.location=location
    cam.data.lens=lens
    cam.rotation_euler=(Vector(target)-cam.location).to_track_quat('-Z','Y').to_euler()


def render_previews(groups):
    scene=bpy.context.scene
    def visible(names):
        for c in groups:
            c.hide_render = c.name not in names
    visible({c.name for c in groups})
    aim_camera((7,-1.5,0.8),(8,-29,17),52)
    scene.render.filepath=os.path.join(PREVIEW,'AssetPack_Overview.png')
    bpy.ops.render.render(write_still=True)
    visible({'CHAR_Humanoid_Modular','CHAR_Void_Quadruped_Modular'})
    aim_camera((-1.35,-0.35,0.85),(-1.4,-7.5,3.6),58)
    scene.render.filepath=os.path.join(PREVIEW,'Characters.png')
    bpy.ops.render.render(write_still=True)
    visible({'ENV_Ship_ModularKit'})
    aim_camera((7.0,-2.0,0.75),(7.0,-13.5,8.2),55)
    scene.render.filepath=os.path.join(PREVIEW,'ShipKit.png')
    bpy.ops.render.render(write_still=True)
    visible({'ENV_Space_Asteroids'})
    aim_camera((15.0,-1.0,0.75),(15.0,-11.5,6.0),58)
    scene.render.filepath=os.path.join(PREVIEW,'Asteroids.png')
    bpy.ops.render.render(write_still=True)
    visible({c.name for c in groups})


def write_docs():
    data={
      'pack':'DarkSciFi_Modular_v1_1','units':'meters','fps':30,'forward_axis':'-Y','up_axis':'+Z',
      'humanoid':{'rig':'RIG_Humanoid_7Head','height_m':1.82,'socket_standard':'HUM_v1','animations':{
        'walk':{'action':'HUM_Walk_Loop','loop':True,'blend_in_frames':5,'blend_out_frames':5},
        'run':{'action':'HUM_Run_Loop','loop':True,'blend_in_frames':5,'blend_out_frames':5},
        'turn_left':'HUM_Turn_Left_90','turn_right':'HUM_Turn_Right_90','attack':'HUM_Attack_Melee','jump':'HUM_Jump',
        'walk_to_run':{'action':'HUM_Walk_To_Run','then':'HUM_Run_Loop'},'run_to_walk':{'action':'HUM_Run_To_Walk','then':'HUM_Walk_Loop'},
        'directional_pose_blend':['HUM_Pose_Turn_Neg45','HUM_Pose_Turn_Zero','HUM_Pose_Turn_Pos45']},
        'recommended_layers':['locomotion base','directional pose additive','upper-body attack override']},
      'void_quadruped':{'rig':'RIG_Void_Quadruped','socket_standard':'VOID_v1','animations':{
        'walk':{'action':'VOID_Walk_Loop','loop':True},'run':{'action':'VOID_Run_Loop','loop':True},'turn_left':'VOID_Turn_Left_90','turn_right':'VOID_Turn_Right_90','attack':'VOID_Attack_Pounce','jump':'VOID_Jump','walk_to_run':'VOID_Walk_To_Run','run_to_walk':'VOID_Run_To_Walk','directional_pose_blend':['VOID_Pose_Turn_Neg45','VOID_Pose_Turn_Zero','VOID_Pose_Turn_Pos45']}},
      'modular_rules':{'human':'Enable exactly one base head; face, helmet, chest armor and equipment are optional overlays. All HUM_v1 parts share the same skeleton and origin.','void':'Base body stays enabled; horn, eye cluster, heavy carapace and tail blade are swappable VOID_v1 overlays.','ship':'2 m wall modules and 2 m floor tiles snap on a 0.25 m grid.'},
      'export_notes':['Apply no transforms to armatures before export.','Export deform bones only when supported.','Keep action names unchanged so the animation state map remains valid.','Procedural materials should be baked or replaced with engine PBR textures during integration.']}
    with open(os.path.join(DOCS,'AnimationAndAssembly.json'),'w',encoding='utf-8') as f: json.dump(data,f,ensure_ascii=False,indent=2)
    readme='''# Dark Sci-Fi Modular Blender Asset Pack\n\n- Blender 5.2 authoring source, metric units, +Z up, -Y forward.\n- Low-poly modular humanoid uses a 7-head heroic proportion and HUM_v1 shared skeleton.\n- Void quadruped is an original low-center, four-legged assault creature with VOID_v1 modular overlays.\n- Ship walls and floor tiles use a 2 m module and 0.25 m snap grid.\n- Materials use dark gunmetal, oxidized armor, cold cyan displays, warning amber, desaturated skin and violet-red alien tissue.\n- Animation clips include walk, run, left/right turn, attack, jump, turn interpolation poses, and walk/run transitions.\n\n## Libraries\n\n- `DarkSciFi_ModularAssetPack.blend`: complete source and showcase scene.\n- `Libraries/Humanoid_Modular.blend`: human rig, body parts, clothing, equipment, actions.\n- `Libraries/Void_Quadruped.blend`: quadruped rig, modular parts, actions.\n- `Libraries/Ship_ModularKit.blend`: walls, controls, medical devices, floor tiles/materials.\n- `Libraries/Space_Asteroids.blend`: four low-poly asteroids and drift-spin actions.\n\nSee `AnimationAndAssembly.json` for clip mapping and blend recommendations.\n'''
    with open(os.path.join(DOCS,'README.md'),'w',encoding='utf-8') as f:f.write(readme)


def main():
    clean();materials();setup_scene();root=coll('DarkSciFi_ModularAssetPack')
    hum,ha=build_humanoid(root);void,va=build_void(root);ship=build_ship(root);space=build_space(root)
    camera_and_light();write_docs()
    save_libraries(root,{'Humanoid_Modular.blend':[hum],'Void_Quadruped.blend':[void],'Ship_ModularKit.blend':[ship],'Space_Asteroids.blend':[space]})
    render_previews([hum,void,ship,space])
    print('ASSET_PACK_COMPLETE',ROOT)


if __name__=='__main__':
    main()
