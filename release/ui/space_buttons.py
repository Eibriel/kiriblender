
import bpy

class Buttons_HT_header(bpy.types.Header):
	__space_type__ = "BUTTONS_WINDOW"

	def draw(self, context):
		layout = self.layout
		
		so = context.space_data
		scene = context.scene

		layout.template_header()

		if context.area.show_menus:
			row = layout.row(align=True)
			row.itemM("Buttons_MT_view", text="View")
			
		row = layout.row()
		row.itemR(so, "buttons_context", expand=True, text="")
		row.itemR(scene, "current_frame")

class Buttons_MT_view(bpy.types.Menu):
	__space_type__ = "BUTTONS_WINDOW"
	__label__ = "View"

	def draw(self, context):
		layout = self.layout
		so = context.space_data

		col = layout.column()
		col.itemR(so, "panel_alignment", expand=True)

bpy.types.register(Buttons_HT_header)
bpy.types.register(Buttons_MT_view)
