"""Splits a binary FBX with several takes into one FBX per take (the mesh, materials and everything else kept).

Interchange imports only the first take of a rigid mesh (a mesh node animated directly, no bones), such as
Wrench.fbx, so each take is imported from its own file. Pure Python: runs in the editor or any Python 3.

    split_takes('Wrench.fbx', 'out_dir', 'A_Wrench_')  ->  {'Equip': 'out_dir/A_Wrench_Equip.fbx', ...}

A take's short name is the part after the last '|' and the last 'Wrench_'-style prefix the artist's tool
adds: 'Wrench|Wrench_Equip' -> 'Equip'.
"""
import os
import struct
import zlib

FOOTER_MAGIC = b'\xf8\x5a\x8c\x6a\xde\xf5\xd9\x7e\xec\xe9\x0c\xe3\x75\x8f\x29\x0b'
ANIM_TYPES = ('AnimationStack', 'AnimationLayer', 'AnimationCurveNode', 'AnimationCurve')


class Node:
    def __init__(self, name, props, raw, children, sentinel):
        self.name = name          # str
        self.props = props        # decoded values (scalars, str, bytes, lists)
        self.raw = raw            # the property bytes exactly as read
        self.children = children  # [Node]
        self.sentinel = sentinel  # the record ended with a null record


def _read_props(d, o, n):
    props = []
    for _ in range(n):
        t = chr(d[o]); o += 1
        if t in 'CYILFD':
            fmt = {'C': '<?', 'Y': '<h', 'I': '<i', 'L': '<q', 'F': '<f', 'D': '<d'}[t]
            size = struct.calcsize(fmt)
            props.append(struct.unpack(fmt, d[o:o + size])[0]); o += size
        elif t in 'SR':
            length = struct.unpack('<I', d[o:o + 4])[0]; o += 4
            v = d[o:o + length]; o += length
            props.append(v.decode('utf-8', 'replace') if t == 'S' else v)
        else:
            count, encoding, compressed = struct.unpack('<III', d[o:o + 12]); o += 12
            data = d[o:o + compressed]; o += compressed
            if encoding:
                data = zlib.decompress(data)
            props.append(list(struct.unpack('<%d%s' % (count, {'f': 'f', 'd': 'd', 'l': 'q', 'i': 'i', 'b': '?'}[t]), data)))
    return props


def _tokens(raw):
    # The raw property list split into (type, bytes) per property.
    o, out = 0, []
    while o < len(raw):
        t = chr(raw[o]); s = o; o += 1
        if t in 'CYILFD':
            o += {'C': 1, 'Y': 2, 'I': 4, 'L': 8, 'F': 4, 'D': 8}[t]
        elif t in 'SR':
            o += 4 + struct.unpack('<I', raw[o:o + 4])[0]
        else:
            o += 12 + struct.unpack('<III', raw[o:o + 12])[2]
        out.append((t, raw[s:o]))
    return out


def _with_p70(node, values):
    """A copy of node whose Properties70 entries named in values ({name: [numbers]}) hold those numbers."""
    children = []
    for c in node.children:
        if c.name == 'Properties70':
            entries = []
            for p in c.children:
                if p.name == 'P' and p.props[0] in values:
                    tokens = _tokens(p.raw)
                    head = b''.join(b for _, b in tokens[:4])
                    tail = b''.join(t.encode() + struct.pack({'D': '<d', 'F': '<f', 'L': '<q', 'I': '<i'}[t], v)
                        for (t, _), v in zip(tokens[4:], values[p.props[0]]))
                    p = Node('P', p.props[:4] + list(values[p.props[0]]), head + tail, [], p.sentinel)
                entries.append(p)
            c = Node(c.name, c.props, c.raw, entries, c.sentinel)
        children.append(c)
    return Node(node.name, node.props, node.raw, children, node.sentinel)


def _p70(node, name):
    for c in node.children:
        if c.name == 'Properties70':
            for p in c.children:
                if p.name == 'P' and p.props[0] == name:
                    return p.props[4:]
    return None


def _read(d, o, wide):
    head = 24 if wide else 12
    end, n, plen = struct.unpack('<QQQ' if wide else '<III', d[o:o + head]); o += head
    name_len = d[o]; o += 1
    if end == 0:
        return None, o
    name = d[o:o + name_len].decode('latin1'); o += name_len
    raw = d[o:o + plen]
    props = _read_props(d, o, n)
    o += plen
    children, sentinel = [], False
    while o < end:
        child, o = _read(d, o, wide)
        if child is None:
            sentinel = True
            break
        children.append(child)
    return Node(name, props, raw, children, sentinel), end


def _write(node, out, base, wide):
    head = 24 if wide else 12
    null = b'\x00' * (head + 1)
    name = node.name.encode('latin1')
    body = bytearray()
    start = base + head + 1 + len(name) + len(node.raw)
    for child in node.children:
        body += _write(child, None, start + len(body), wide)
    if node.sentinel or node.children:
        body += null
    end = start + len(body)
    n = _count_props(node.raw)
    record = struct.pack('<QQQ' if wide else '<III', end, n, len(node.raw)) + bytes([len(name)]) + name + node.raw + body
    return record


def _count_props(raw):
    # Number of properties in a raw property list (types decide the sizes).
    o, n = 0, 0
    while o < len(raw):
        t = chr(raw[o]); o += 1
        if t in 'CYILFD':
            o += {'C': 1, 'Y': 2, 'I': 4, 'L': 8, 'F': 4, 'D': 8}[t]
        elif t in 'SR':
            o += 4 + struct.unpack('<I', raw[o:o + 4])[0]
        else:
            o += 12 + struct.unpack('<III', raw[o:o + 12])[2]
        n += 1
    return n


def _string_prop(s):
    b = s.encode('utf-8')
    return b'S' + struct.pack('<I', len(b)) + b


def read_fbx(path):
    d = open(path, 'rb').read()
    if not d.startswith(b'Kaydara FBX Binary'):
        raise ValueError(path + ' is not a binary FBX')
    version = struct.unpack('<I', d[23:27])[0]
    wide = version >= 7500
    o, top = 27, []
    while True:
        node, o = _read(d, o, wide)
        if node is None:
            break
        top.append(node)
    footer_id = d[o:o + 16]
    return version, top, footer_id


def write_fbx(path, version, top, footer_id):
    wide = version >= 7500
    out = bytearray(d_header(version))
    for node in top:
        out += _write(node, None, len(out), wide)
    out += b'\x00' * (25 if wide else 13)
    out += footer_id + b'\x00' * 4
    pad = ((len(out) + 15) & ~15) - len(out)
    out += b'\x00' * (pad or 16)
    out += struct.pack('<I', version) + b'\x00' * 120 + FOOTER_MAGIC
    open(path, 'wb').write(bytes(out))


def d_header(version):
    return b'Kaydara FBX Binary  \x00\x1a\x00' + struct.pack('<I', version)


def short_take_name(full):
    name = full.split('\x00')[0].split('|')[-1]
    return name.split('_', 1)[1] if '_' in name else name


KTIME_PER_SECOND = 46186158000


def take_lengths(path):
    """{short take name: length in seconds}."""
    _, top, _ = read_fbx(path)
    objects = next(n for n in top if n.name == 'Objects')
    return {short_take_name(o.props[1]): (_p70(o, 'LocalStop') or _p70(o, 'ReferenceStop') or [0])[0] / KTIME_PER_SECOND
        for o in objects.children if o.name == 'AnimationStack'}


def split_takes(path, out_dir, prefix):
    version, top, footer_id = read_fbx(path)
    nodes = {n.name: n for n in top}
    objects, connections = nodes['Objects'], nodes['Connections']
    stacks = [o for o in objects.children if o.name == 'AnimationStack']

    # Which anim objects hang under each stack (stack <- layer <- curve node <- curve: child, parent).
    links = [(c.props[1], c.props[2]) for c in connections.children]
    def under(root):
        found, todo = set(), [root]
        while todo:
            parent = todo.pop()
            for child, p in links:
                if p == parent and child not in found:
                    kind = next((o.name for o in objects.children if o.props and o.props[0] == child), None)
                    if kind in ANIM_TYPES:
                        found.add(child); todo.append(child)
        return found
    owned = {s.props[0]: under(s.props[0]) | {s.props[0]} for s in stacks}

    os.makedirs(out_dir, exist_ok=True)
    written = {}
    for stack in stacks:
        keep = owned[stack.props[0]]
        drop = set().union(*(ids for sid, ids in owned.items() if sid != stack.props[0])) - keep
        full_name = stack.props[1].split('\x00')[0]
        short = short_take_name(full_name)

        # Interchange gives a channel this take keeps constant the node's own (rest) value, not the take's:
        # the rest transform of each animated node becomes this take's first frame.
        by_id = {o.props[0]: o for o in objects.children if o.props}
        rest = {}  # model id -> {'Lcl Translation': [x, y, z], ...}
        for c in connections.children:
            if c.props[0] == 'OP' and c.props[1] in keep and by_id.get(c.props[1]) and by_id[c.props[1]].name == 'AnimationCurveNode':
                curve_node, model, channel = c.props[1], c.props[2], c.props[3]
                value = list(_p70(by_id[model], channel) or [0.0, 0.0, 0.0])
                for cc in connections.children:
                    if cc.props[0] == 'OP' and cc.props[2] == curve_node and cc.props[1] in by_id:
                        keys = next((k.props[0] for k in by_id[cc.props[1]].children if k.name == 'KeyValueFloat'), None)
                        axis = 'XYZ'.index(cc.props[3][-1])
                        if keys:
                            value[axis] = keys[0]
                rest.setdefault(model, {})[channel] = value
        # The timeline is the take's own length (Interchange's default range).
        stop = _p70(stack, 'LocalStop') or _p70(stack, 'ReferenceStop')

        new_objects = Node('Objects', objects.props, objects.raw,
            [_with_p70(o, rest[o.props[0]]) if o.props and o.props[0] in rest else o
             for o in objects.children if not (o.props and o.props[0] in drop)], objects.sentinel)
        new_conns = Node('Connections', connections.props, connections.raw,
            [c for c in connections.children if c.props[1] not in drop and c.props[2] not in drop], connections.sentinel)
        new_top = []
        for node in top:
            if node.name == 'Objects':
                node = new_objects
            elif node.name == 'Connections':
                node = new_conns
            elif node.name == 'Takes':
                takes = [t for t in node.children if t.name != 'Take' or t.props[0] == full_name]
                current = Node('Current', [full_name], _string_prop(full_name), [], False)
                takes = [current if t.name == 'Current' else t for t in takes]
                node = Node('Takes', node.props, node.raw, takes, node.sentinel)
            elif node.name == 'Definitions':
                node = _recount(node, new_objects)
            elif node.name == 'GlobalSettings' and stop:
                node = _with_p70(node, {'TimeSpanStart': [0], 'TimeSpanStop': stop})
            new_top.append(node)
        out = os.path.join(out_dir, prefix + short + '.fbx')
        write_fbx(out, version, new_top, footer_id)
        written[short] = out
    return written


def _recount(definitions, objects):
    counts = {}
    for o in objects.children:
        counts[o.name] = counts.get(o.name, 0) + 1
    counts['GlobalSettings'] = 1
    total = 0
    children = []
    for c in definitions.children:
        if c.name == 'ObjectType':
            kind = c.props[0]
            sub = []
            for s in c.children:
                if s.name == 'Count' and kind in ANIM_TYPES:
                    n = counts.get(kind, 0)
                    s = Node('Count', [n], b'I' + struct.pack('<i', n), [], s.sentinel)
                if s.name == 'Count':
                    total += s.props[0]
                sub.append(s)
            c = Node(c.name, c.props, c.raw, sub, c.sentinel)
        children.append(c)
    children = [Node('Count', [total], b'I' + struct.pack('<i', total), [], c.sentinel) if c.name == 'Count' else c for c in children]
    return Node(definitions.name, definitions.props, definitions.raw, children, definitions.sentinel)
