"""Generate the README's .vkmesh v4 diagram from the serialized field order.

Keep in sync with src/mesh.cpp save_mesh_data and include/vulkanizer/mesh.hpp.
Run from any directory: python docs/images/generate_vkmesh_layout.py
Requires Pillow for the PNG preview; the SVG is used by the README.
"""
from pathlib import Path
from html import escape
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent
W, H = 1400, 1570
image = Image.new('RGB', (W, H), '#f8fafc')
draw = ImageDraw.Draw(image)
svg = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}" role="img" aria-labelledby="title desc">',
       '<title id="title">Vulkanizer .vkmesh version 4 binary file layout</title>',
       '<desc id="desc">A protocol-style 32-bit field diagram. Fixed header fields are followed by vertex layout, meshes, bounds, indices, vertices, instances, lights, and embedded images. Scalars use native byte order, not network byte order. Variable blocks are collapsed.</desc>',
       f'<rect width="{W}" height="{H}" fill="#f8fafc"/>']

def rect(x, y, w, h, fill, stroke='#94a3b8'):
    draw.rectangle((x, y, x+w, y+h), fill=fill, outline=stroke, width=1)
    svg.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="{fill}" stroke="{stroke}"/>')

def text(x, y, value, size=20, color='#0f172a', anchor='left', bold=False):
    font = ImageFont.truetype('C:/Windows/Fonts/' + ('consolab.ttf' if bold else 'consola.ttf'), size)
    # y is the top of the line for both renderers.
    draw.text((x, y), value, font=font, fill=color, anchor={'left':'lt','middle':'mt','right':'rt'}[anchor])
    svg.append(f'<text x="{x}" y="{y+size*0.8}" font-family="Consolas, monospace" font-size="{size}" font-weight="{700 if bold else 400}" text-anchor="{dict(left="start",middle="middle",right="end")[anchor]}" fill="{color}">{escape(value)}</text>')

def line(x1,y1,x2,y2,color='#94a3b8'):
    draw.line((x1,y1,x2,y2), fill=color)
    svg.append(f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="{color}"/>')

text(64, 40, '.vkmesh / binary layout', 36, bold=True)
text(64, 92, 'VERSION 4  |  serialized order  |  32-bit scalar fields', 21, '#475569')
text(64, 129, 'Native-endian cache (little-endian on current Windows builds), not network byte order.', 18, '#475569')
x, width, y = 214, 1120, 219
text(192, 178, 'BYTE OFFSET', 16, '#475569', 'right')
for bit in range(32):
    bx = x + (bit + 0.5) * width / 32
    text(bx, 178, str(bit), 16, '#475569', 'middle')
    line(x + bit * width/32, 204, x + bit * width/32, 217)
line(x+width,204,x+width,217)
text(64, 152, 'Ruler positions show field width; they do not specify integer bit significance.', 16, '#64748b')

rows = [
 ('0', 'magic_value = 0x475A4B56 / VKZG', 'uint32', 'header'),
 ('4', 'mesh_count (M)', 'uint32', 'header'),
 ('8', 'index_data_size (bytes)', 'uint32', 'header'),
 ('12', 'vertex_data_size (bytes)', 'uint32', 'header'),
 ('16', 'stream_version = 0x565A0004', 'uint32', 'layout'),
 ('20', 'binding_count (B)', 'uint32', 'layout'),
 ('24', 'vertex bindings [B]', 'B x 12 bytes', 'array'),
 ('variable', 'attribute_count (A)', 'uint32', 'layout'),
 ('', 'vertex attributes [A]', 'A x 16 bytes', 'array'),
 ('', 'base meshes [M]', 'M x 52 bytes', 'array'),
 ('', 'local bounding boxes [M]', 'M x 24 bytes', 'array'),
 ('', 'index data / uint32 indices', 'index_data_size bytes', 'array'),
 ('', 'interleaved vertex data', 'vertex_data_size bytes', 'array'),
 ('', 'instance_count (I)', 'uint32', 'count'),
 ('', 'mesh instances [I]', 'I x 136 bytes', 'array'),
 ('', 'light_count (L)', 'uint32', 'count'),
 ('', 'placed lights [L]', 'L x 56 bytes', 'array'),
 ('', 'image_count (T)', 'uint32', 'count'),
 ('', 'image records [T] / embedded KTX2', 'variable length', 'array'),
]
colors = {'header':'#dbeafe', 'layout':'#e0f2fe', 'count':'#d1fae5', 'array':'#ffffff'}
for offset, label, detail, kind in rows:
    height = 46
    rect(x,y,width,height,colors[kind])
    if kind == 'array':
        # Two slash marks explicitly denote a collapsed multiword block.
        text(x+18,y+13,'//',19,'#64748b')
    text(x+58,y+13,label,20,bold=(kind!='array'))
    text(x+width-18,y+14,detail,17,'#475569','right')
    if offset: text(192,y+15,offset,17,'#475569','right')
    y += height
text(214,y+16,'// = multiword block, collapsed vertically; no padding is added between sections.',17,'#475569')
text(214,y+43,'Offsets after the binding array depend on preceding counts and byte lengths.',17,'#475569')

# Record expansions are byte counts, deliberately separate from the bit ruler.
y += 95
text(64,y,'INSTANCE RECORD / 136 bytes',22,bold=True)
text(734,y,'IMAGE RECORD / repeated T times',22,bold=True)
y += 40
instance = [('model / column-major mat4','64 bytes'),('model_inverse / mat4','64 bytes'),('mesh_id / uint32','4 bytes'),('material_id / uint32','4 bytes')]
images = [('uri_byte_count / uint32','4 bytes'),('URI bytes / no terminator','variable'),('mime_byte_count + MIME bytes','4 + variable'),('payload_byte_count / uint32','4 bytes'),('encoded payload / complete KTX2','variable')]
for col, records in [(64,instance),(734,images)]:
    yy=y
    for label, count in records:
        rect(col,yy,600,38,'#ffffff')
        text(col+12,yy+11,label,17)
        text(col+587,yy+11,count,16,'#475569','right')
        yy+=38
text(64,y+166,'material_id = 0xffffffff inherits the base material.',16,'#475569')
text(64, H-90,'Record sizes shown for the current unaligned GLM/native ABI. Geometry arrays are raw records.',17,'#475569')
text(64, H-61,'Companion files: .vkmat = materials + samplers + texture references; .vkbounds = bounds copy.',17,'#475569')
text(64, H-32,'Source of truth: src/mesh.cpp :: save_mesh_data() / include/vulkanizer/mesh.hpp',16,'#64748b')
svg.append('</svg>')
(ROOT / 'vkmesh-layout.svg').write_text('\n'.join(svg), encoding='utf-8')
image.save(ROOT / 'vkmesh-layout.png')
print(f'Wrote {ROOT / "vkmesh-layout.svg"} and PNG preview')
