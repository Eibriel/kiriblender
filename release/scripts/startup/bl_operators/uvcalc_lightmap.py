# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

import bpy
import mathutils


class prettyface(object):
    __slots__ = "uv", "width", "height", "children", "xoff", "yoff", "has_parent", "rot"

    def __init__(self, data):
        self.has_parent = False
        self.rot = False  # only used for triables
        self.xoff = 0
        self.yoff = 0

        if type(data) == list:  # list of data
            self.uv = None

            # join the data
            if len(data) == 2:
                # 2 vertical blocks
                data[1].xoff = data[0].width
                self.width = data[0].width * 2
                self.height = data[0].height

            elif len(data) == 4:
                # 4 blocks all the same size
                d = data[0].width  # dimension x/y are the same

                data[1].xoff += d
                data[2].yoff += d

                data[3].xoff += d
                data[3].yoff += d

                self.width = self.height = d * 2

            #else:
            #    print(len(data), data)
            #    raise "Error"

            for pf in data:
                pf.has_parent = True

            self.children = data

        elif type(data) == tuple:
            # 2 blender faces
            # f, (len_min, len_mid, len_max)
            self.uv = data

            f1, lens1, lens1ord = data[0]
            if data[1]:
                f2, lens2, lens2ord = data[1]
                self.width = (lens1[lens1ord[0]] + lens2[lens2ord[0]]) / 2.0
                self.height = (lens1[lens1ord[1]] + lens2[lens2ord[1]]) / 2.0
            else:  # 1 tri :/
                self.width = lens1[0]
                self.height = lens1[1]

            self.children = []

        else:  # blender face
            # self.uv = data.uv
            self.uv = data.id_data.uv_textures.active.data[data.index].uv  # XXX25

            # cos = [v.co for v in data]
            cos = [data.id_data.vertices[v].co for v in data.vertices]  # XXX25

            self.width = ((cos[0] - cos[1]).length + (cos[2] - cos[3]).length) / 2.0
            self.height = ((cos[1] - cos[2]).length + (cos[0] - cos[3]).length) / 2.0

            self.children = []

    def spin(self):
        if self.uv and len(self.uv) == 4:
            self.uv = self.uv[1], self.uv[2], self.uv[3], self.uv[0]

        self.width, self.height = self.height, self.width
        self.xoff, self.yoff = self.yoff, self.xoff  # not needed?
        self.rot = not self.rot  # only for tri pairs.
        # print("spinning")
        for pf in self.children:
            pf.spin()

    def place(self, xoff, yoff, xfac, yfac, margin_w, margin_h):
        from math import pi

        xoff += self.xoff
        yoff += self.yoff

        for pf in self.children:
            pf.place(xoff, yoff, xfac, yfac, margin_w, margin_h)

        uv = self.uv
        if not uv:
            return

        x1 = xoff
        y1 = yoff
        x2 = xoff + self.width
        y2 = yoff + self.height

        # Scale the values
        x1 = x1 / xfac + margin_w
        x2 = x2 / xfac - margin_w
        y1 = y1 / yfac + margin_h
        y2 = y2 / yfac - margin_h

        # 2 Tri pairs
        if len(uv) == 2:
            # match the order of angle sizes of the 3d verts with the UV angles and rotate.
            def get_tri_angles(v1, v2, v3):
                a1 = (v2 - v1).angle(v3 - v1, pi)
                a2 = (v1 - v2).angle(v3 - v2, pi)
                a3 = pi - (a1 + a2)  # a3= (v2 - v3).angle(v1 - v3)

                return [(a1, 0), (a2, 1), (a3, 2)]

            def set_uv(f, p1, p2, p3):

                # cos =
                #v1 = cos[0]-cos[1]
                #v2 = cos[1]-cos[2]
                #v3 = cos[2]-cos[0]

                # angles_co = get_tri_angles(*[v.co for v in f])
                angles_co = get_tri_angles(*[f.id_data.vertices[v].co for v in f.vertices])  # XXX25

                angles_co.sort()
                I = [i for a, i in angles_co]

                # fuv = f.uv
                fuv = f.id_data.uv_textures.active.data[f.index].uv  # XXX25

                if self.rot:
                    fuv[I[2]] = p1
                    fuv[I[1]] = p2
                    fuv[I[0]] = p3
                else:
                    fuv[I[2]] = p1
                    fuv[I[0]] = p2
                    fuv[I[1]] = p3

            f, lens, lensord = uv[0]

            set_uv(f, (x1, y1), (x1, y2 - margin_h), (x2 - margin_w, y1))

            if uv[1]:
                f, lens, lensord = uv[1]
                set_uv(f, (x2, y2), (x2, y1 + margin_h), (x1 + margin_w, y2))

        else:  # 1 QUAD
            uv[1][0], uv[1][1] = x1, y1
            uv[2][0], uv[2][1] = x1, y2
            uv[3][0], uv[3][1] = x2, y2
            uv[0][0], uv[0][1] = x2, y1

    def __hash__(self):
        # None unique hash
        return self.width, self.height


def lightmap_uvpack(meshes,
                      PREF_SEL_ONLY=True,
                      PREF_NEW_UVLAYER=False,
                      PREF_PACK_IN_ONE=False,
                      PREF_APPLY_IMAGE=False,
                      PREF_IMG_PX_SIZE=512,
                      PREF_BOX_DIV=8,
                      PREF_MARGIN_DIV=512
                      ):
    '''
    BOX_DIV if the maximum division of the UV map that
    a box may be consolidated into.
    Basicly, a lower value will be slower but waist less space
    and a higher value will have more clumpy boxes but more waisted space
    '''
    import time
    from math import sqrt

    if not meshes:
        return

    t = time.time()

    if PREF_PACK_IN_ONE:
        if PREF_APPLY_IMAGE:
            image = bpy.data.images.new(name="lightmap", width=PREF_IMG_PX_SIZE, height=PREF_IMG_PX_SIZE, alpha=False)
        face_groups = [[]]
    else:
        face_groups = []

    for me in meshes:
        # Add face UV if it does not exist.
        # All new faces are selected.
        if not me.uv_textures:
            me.uv_textures.new()

        if PREF_SEL_ONLY:
            faces = [f for f in me.faces if f.select]
        else:
            faces = me.faces[:]

        if PREF_PACK_IN_ONE:
            face_groups[0].extend(faces)
        else:
            face_groups.append(faces)

        if PREF_NEW_UVLAYER:
            me.uv_textures.new()

    for face_sel in face_groups:
        print("\nStarting unwrap")

        if len(face_sel) < 4:
            print("\tWarning, less then 4 faces, skipping")
            continue

        pretty_faces = [prettyface(f) for f in face_sel if len(f.vertices) == 4]

        # Do we have any tri's
        if len(pretty_faces) != len(face_sel):

            # Now add tri's, not so simple because we need to pair them up.
            def trylens(f):
                # f must be a tri

                # cos = [v.co for v in f]
                cos = [f.id_data.vertices[v].co for v in f.vertices]  # XXX25

                lens = [(cos[0] - cos[1]).length, (cos[1] - cos[2]).length, (cos[2] - cos[0]).length]

                lens_min = lens.index(min(lens))
                lens_max = lens.index(max(lens))
                for i in range(3):
                    if i != lens_min and i != lens_max:
                        lens_mid = i
                        break
                lens_order = lens_min, lens_mid, lens_max

                return f, lens, lens_order

            tri_lengths = [trylens(f) for f in face_sel if len(f.vertices) == 3]
            del trylens

            def trilensdiff(t1, t2):
                return\
                abs(t1[1][t1[2][0]] - t2[1][t2[2][0]]) + \
                abs(t1[1][t1[2][1]] - t2[1][t2[2][1]]) + \
                abs(t1[1][t1[2][2]] - t2[1][t2[2][2]])

            while tri_lengths:
                tri1 = tri_lengths.pop()

                if not tri_lengths:
                    pretty_faces.append(prettyface((tri1, None)))
                    break

                best_tri_index = -1
                best_tri_diff = 100000000.0

                for i, tri2 in enumerate(tri_lengths):
                    diff = trilensdiff(tri1, tri2)
                    if diff < best_tri_diff:
                        best_tri_index = i
                        best_tri_diff = diff

                pretty_faces.append(prettyface((tri1, tri_lengths.pop(best_tri_index))))

        # Get the min, max and total areas
        max_area = 0.0
        min_area = 100000000.0
        tot_area = 0
        for f in face_sel:
            area = f.area
            if area > max_area:
                max_area = area
            if area < min_area:
                min_area = area
            tot_area += area

        max_len = sqrt(max_area)
        min_len = sqrt(min_area)
        side_len = sqrt(tot_area)

        # Build widths

        curr_len = max_len

        print("\tGenerating lengths...", end="")

        lengths = []
        while curr_len > min_len:
            lengths.append(curr_len)
            curr_len = curr_len / 2.0

            # Dont allow boxes smaller then the margin
            # since we contract on the margin, boxes that are smaller will create errors
            # print(curr_len, side_len/MARGIN_DIV)
            if curr_len / 4.0 < side_len / PREF_MARGIN_DIV:
                break

        if not lengths:
            lengths.append(curr_len)

        # convert into ints
        lengths_to_ints = {}

        l_int = 1
        for l in reversed(lengths):
            lengths_to_ints[l] = l_int
            l_int *= 2

        lengths_to_ints = list(lengths_to_ints.items())
        lengths_to_ints.sort()
        print("done")

        # apply quantized values.

        for pf in pretty_faces:
            w = pf.width
            h = pf.height
            bestw_diff = 1000000000.0
            besth_diff = 1000000000.0
            new_w = 0.0
            new_h = 0.0
            for l, i in lengths_to_ints:
                d = abs(l - w)
                if d < bestw_diff:
                    bestw_diff = d
                    new_w = i  # assign the int version

                d = abs(l - h)
                if d < besth_diff:
                    besth_diff = d
                    new_h = i  # ditto

            pf.width = new_w
            pf.height = new_h

            if new_w > new_h:
                pf.spin()

        print("...done")

        # Since the boxes are sized in powers of 2, we can neatly group them into bigger squares
        # this is done hierarchily, so that we may avoid running the pack function
        # on many thousands of boxes, (under 1k is best) because it would get slow.
        # Using an off and even dict us usefull because they are packed differently
        # where w/h are the same, their packed in groups of 4
        # where they are different they are packed in pairs
        #
        # After this is done an external pack func is done that packs the whole group.

        print("\tConsolidating Boxes...", end="")
        even_dict = {}  # w/h are the same, the key is an int (w)
        odd_dict = {}  # w/h are different, the key is the (w,h)

        for pf in pretty_faces:
            w, h = pf.width, pf.height
            if w == h:
                even_dict.setdefault(w, []).append(pf)
            else:
                odd_dict.setdefault((w, h), []).append(pf)

        # Count the number of boxes consolidated, only used for stats.
        c = 0

        # This is tricky. the total area of all packed boxes, then squt that to get an estimated size
        # this is used then converted into out INT space so we can compare it with
        # the ints assigned to the boxes size
        # and divided by BOX_DIV, basicly if BOX_DIV is 8
        # ...then the maximum box consolidataion (recursive grouping) will have a max width & height
        # ...1/8th of the UV size.
        # ...limiting this is needed or you end up with bug unused texture spaces
        # ...however if its too high, boxpacking is way too slow for high poly meshes.
        float_to_int_factor = lengths_to_ints[0][0]
        if float_to_int_factor > 0:
            max_int_dimension = int(((side_len / float_to_int_factor)) / PREF_BOX_DIV)
            ok = True
        else:
            max_int_dimension = 0.0  # wont be used
            ok = False

        # RECURSIVE prettyface grouping
        while ok:
            ok = False

            # Tall boxes in groups of 2
            for d, boxes in list(odd_dict.items()):
                if d[1] < max_int_dimension:
                    #\boxes.sort(key = lambda a: len(a.children))
                    while len(boxes) >= 2:
                        # print("foo", len(boxes))
                        ok = True
                        c += 1
                        pf_parent = prettyface([boxes.pop(), boxes.pop()])
                        pretty_faces.append(pf_parent)

                        w, h = pf_parent.width, pf_parent.height

                        if w > h:
                            raise "error"

                        if w == h:
                            even_dict.setdefault(w, []).append(pf_parent)
                        else:
                            odd_dict.setdefault((w, h), []).append(pf_parent)

            # Even boxes in groups of 4
            for d, boxes in list(even_dict.items()):
                if d < max_int_dimension:
                    boxes.sort(key=lambda a: len(a.children))

                    while len(boxes) >= 4:
                        # print("bar", len(boxes))
                        ok = True
                        c += 1

                        pf_parent = prettyface([boxes.pop(), boxes.pop(), boxes.pop(), boxes.pop()])
                        pretty_faces.append(pf_parent)
                        w = pf_parent.width  # width and weight are the same
                        even_dict.setdefault(w, []).append(pf_parent)

        del even_dict
        del odd_dict

        orig = len(pretty_faces)

        pretty_faces = [pf for pf in pretty_faces if not pf.has_parent]

        # spin every second prettyface
        # if there all vertical you get less efficiently used texture space
        i = len(pretty_faces)
        d = 0
        while i:
            i -= 1
            pf = pretty_faces[i]
            if pf.width != pf.height:
                d += 1
                if d % 2:  # only pack every second
                    pf.spin()
                    # pass

        print("Consolidated", c, "boxes, done")
        # print("done", orig, len(pretty_faces))

        # boxes2Pack.append([islandIdx, w,h])
        print("\tPacking Boxes", len(pretty_faces), end="...")
        boxes2Pack = [[0.0, 0.0, pf.width, pf.height, i] for i, pf in enumerate(pretty_faces)]
        packWidth, packHeight = mathutils.geometry.box_pack_2d(boxes2Pack)

        # print(packWidth, packHeight)

        packWidth = float(packWidth)
        packHeight = float(packHeight)

        margin_w = ((packWidth) / PREF_MARGIN_DIV) / packWidth
        margin_h = ((packHeight) / PREF_MARGIN_DIV) / packHeight

        # print(margin_w, margin_h)
        print("done")

        # Apply the boxes back to the UV coords.
        print("\twriting back UVs", end="")
        for i, box in enumerate(boxes2Pack):
            pretty_faces[i].place(box[0], box[1], packWidth, packHeight, margin_w, margin_h)
            # pf.place(box[1][1], box[1][2], packWidth, packHeight, margin_w, margin_h)
        print("done")

        if PREF_APPLY_IMAGE:
            if not PREF_PACK_IN_ONE:
                image = Image.New("lightmap", PREF_IMG_PX_SIZE, PREF_IMG_PX_SIZE, 24)

            for f in face_sel:
                # f.image = image
                f.id_data.uv_textures.active.data[f.index].image = image  # XXX25

    for me in meshes:
        me.update()

    print("finished all %.2f " % (time.time() - t))

    # Window.RedrawAll()


def unwrap(operator, context, **kwargs):

    is_editmode = (bpy.context.object.mode == 'EDIT')
    if is_editmode:
        bpy.ops.object.mode_set(mode='OBJECT', toggle=False)

    PREF_ACT_ONLY = kwargs.pop("PREF_ACT_ONLY")

    meshes = []
    if PREF_ACT_ONLY:
        obj = context.scene.objects.active
        if obj and obj.type == 'MESH':
            meshes = [obj.data]
    else:
        meshes = {me.name: me for obj in context.selected_objects if obj.type == 'MESH' for me in (obj.data,) if not me.library if len(me.faces)}.values()

    if not meshes:
        operator.report({'ERROR'}, "No mesh object.")
        return {'CANCELLED'}

    lightmap_uvpack(meshes, **kwargs)

    if is_editmode:
        bpy.ops.object.mode_set(mode='EDIT', toggle=False)

    return {'FINISHED'}

from bpy.props import BoolProperty, FloatProperty, IntProperty, EnumProperty


class LightMapPack(bpy.types.Operator):
    '''Follow UVs from active quads along continuous face loops'''
    bl_idname = "uv.lightmap_pack"
    bl_label = "Lightmap Pack"
    bl_options = {'REGISTER', 'UNDO'}

    PREF_CONTEXT = bpy.props.EnumProperty(
            items=(("SEL_FACES", "Selected Faces", "Space all UVs evently"),
                   ("ALL_FACES", "All Faces", "Average space UVs edge length of each loop"),
                   ("ALL_OBJECTS", "Selected Mesh Object", "Average space UVs edge length of each loop")
                   ),
            name="Selection",
            description="")

    # Image & UVs...
    PREF_PACK_IN_ONE = BoolProperty(name="Share Tex Space", default=True, description="Objects Share texture space, map all objects into 1 uvmap")
    PREF_NEW_UVLAYER = BoolProperty(name="New UV Layer", default=False, description="Create a new UV layer for every mesh packed")
    PREF_APPLY_IMAGE = BoolProperty(name="New Image", default=False, description="Assign new images for every mesh (only one if shared tex space enabled)")
    PREF_IMG_PX_SIZE = IntProperty(name="Image Size", min=64, max=5000, default=512, description="Width and Height for the new image")

    # UV Packing...
    PREF_BOX_DIV = IntProperty(name="Pack Quality", min=1, max=48, default=12, description="Pre Packing before the complex boxpack")
    PREF_MARGIN_DIV = FloatProperty(name="Margin", min=0.001, max=1.0, default=0.1, description="Size of the margin as a division of the UV")

    def execute(self, context):
        kwargs = self.as_keywords()
        PREF_CONTEXT = kwargs.pop("PREF_CONTEXT")

        if PREF_CONTEXT == 'SEL_FACES':
            kwargs["PREF_ACT_ONLY"] = True
            kwargs["PREF_SEL_ONLY"] = True
        elif PREF_CONTEXT == 'ALL_FACES':
            kwargs["PREF_ACT_ONLY"] = True
            kwargs["PREF_SEL_ONLY"] = False
        elif PREF_CONTEXT == 'ALL_OBJECTS':
            kwargs["PREF_ACT_ONLY"] = False
            kwargs["PREF_SEL_ONLY"] = False
        else:
            raise Exception("invalid context")

        kwargs["PREF_MARGIN_DIV"] = int(1.0 / (kwargs["PREF_MARGIN_DIV"] / 100.0))

        return unwrap(self, context, **kwargs)

    def invoke(self, context, event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self)
