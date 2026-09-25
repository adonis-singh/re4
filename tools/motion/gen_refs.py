#!/usr/bin/env python3
"""Reads the game source (src/) and writes refs.py: which archive entry every MotionSetCore call
plays, from which function, on which model; the file tables that map enemy ids / weapon numbers to
archives; the module -> source file map. character.py uses it to say, for every motion a
character can play, the game function that plays it.

    python3 tools/motion/gen_refs.py          # rewrites tools/motion/refs.py

What is scanned: every `MotionSetCore(m, w, DATA, SEQ, ...)`, `X->motionSet(DATA, hokan, frame, stat, SEQ)`,
`cPlayer::motionSet(D0, S0, D1, S1, hokan, frame)`, `PlRegistMotion(D0..D11)`, `SubCharRegistMotion(D0, D1)`,
`cEm10::setEvtMotion / setGondolaMotion / setR11DMotion(D..)` and `CamCtrl.MotionSet(DATA, ..)` in src/.
The DATA expression is resolved to (archive class, index):
  player   PL_ARC_PTR(pG->pPlayer, n) / PL_DATA_ADDR / pl_mod.h PL_ARC(n): the player archive plNN.drs
  sub      PL_ARC_PTR(pl->subArc, n) (the emNN.cpp PL_ARC(n)): the player's subArc, which is the
           player archive except inside a grab routine that set `pl->subArc = em->subArc` (class em)
  em       PL_ARC_PTR(em->subArc, n) / ARC(n) in an enemy module: that module's emNN.drs (plNN.drs for
           the vehicle modules pl0e / pl0f, the partner modules pl11 / pl14)
  wep      WEP_ARC_PTR(n): the weapon module archive(s) the source file is linked into (modules.py UNITS)
  room     ROOM_ARC_PTR(pG->pRoom, n): the room archive rNNN.das of the file (st*/rNNN.cpp)
  ss       SS_ARC_PTR(arc, n): a sub screen .dat (ss_model.cpp WEP_ARC = ss_wepNN.dat, ss_term.cpp
           pTerm / partner data)
  local    a local (`m0 = ARC(0x116); ... MotionSetCore(.., m0, ..)`) is followed back within the function
Whatever cannot be resolved to a constant entry (a table walked by index, a work field filled by a
room, an event bin) is recorded under `unresolved` with the expression, so nothing is silently dropped.
"""
import collections
import importlib.util
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC = os.path.join(ROOT, 'src')
INCLUDE = os.path.join(ROOT, 'include')
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'refs.py')

CALL = re.compile(r'\b(MotionSetCore|PlRegistMotion|SubCharRegistMotion|setEvtMotion|setGondolaMotion|setR11DMotion)\s*\('
                  r'|(->|\.)(motionSet|MotionSet)\s*\(|\bmot3\.(set|set0)\s*\(')
# a wrapper is followed only when its name says it sets a motion (the generic set / init / move
# names would drag every other class's method in)
WRAPPER_NAME = re.compile(r'Mot|mot|Blend|setDown|ButtonCount')
FUNC_DEF = re.compile(r'^[A-Za-z_][\w:<>*&,\s]*?\b([\w~]+(?:::[\w~]+)?)\s*\([^;{}]*\)\s*(?:const)?\s*\{?\s*$')
ARC_MACRO = re.compile(r'#define\s+(\w+)\((\w+)(?:,\s*(\w+))?\)\s+(.*)')
# global.h WEP_MOT(pl, idx, no) / PLA_MOT: `((pl)->m_MotTbl[idx] = WEP_ARC_PTR(no))`
FILL_MACRO = re.compile(r'#define\s+(\w+)\((\w+),\s*(\w+),\s*(\w+)\)\s+\(\(\2\)->(\w+)\[\3\]\s*=\s*(.*)\)\s*$', re.M)
NUM = r'(0x[0-9A-Fa-f]+|\d+)'


def split_args(s):
    args, depth, cur = [], 0, ''
    for ch in s:
        if ch in '([{':
            depth += 1
        elif ch in ')]}':
            depth -= 1
        if ch == ',' and depth == 0:
            args.append(cur.strip())
            cur = ''
        else:
            cur += ch
    if cur.strip():
        args.append(cur.strip())
    return args


def call_args(text, start):
    depth, i = 1, start
    while depth and i < len(text):
        c = text[i]
        if c == '(':
            depth += 1
        elif c == ')':
            depth -= 1
        i += 1
    return split_args(text[start:i - 1]), i


def const(s):
    s = s.strip()
    m = re.fullmatch(r'\(?\s*' + NUM + r'\s*(/\s*4)?\s*\)?', s)
    if not m:
        return None
    v = int(m.group(1), 0)
    return v // 4 if m.group(2) else v


def header_macros():
    """The ARC-style macros of include/*.h (em.h ARC / EM_ARC, pl_mod.h SUB_ARC ...): every unit
    sees them; a unit's own #define of the same name wins. Also the table-fill macros
    (FILL_MACRO): name -> (field, index param, value param, value expression)."""
    arcs, fills = {}, {}
    for name in sorted(os.listdir(INCLUDE)):
        if name.endswith('.h'):
            text = open(os.path.join(INCLUDE, name), encoding='utf-8', errors='replace').read()
            for m in ARC_MACRO.finditer(text):
                arcs[m.group(1)] = (m.group(2), m.group(3), m.group(4))
            for m in FILL_MACRO.finditer(text):
                fills[m.group(1)] = (m.group(5), m.group(3), m.group(4), m.group(6))
    return arcs, fills


HEADER_MACROS, FILL_MACROS = header_macros()


class File:
    def __init__(self, path):
        self.path = path
        self.rel = os.path.relpath(path, SRC)
        self.text = open(path, encoding='utf-8', errors='replace').read()
        self.lines = self.text.split('\n')
        self.macros = dict(HEADER_MACROS)
        for m in ARC_MACRO.finditer(self.text):
            self.macros[m.group(1)] = (m.group(2), m.group(3), m.group(4))
        self.module = self.rel.split('/')[0]      # em10, wep04, game, st1 ...
        self.stem = None
        if re.fullmatch(r'(em|pl|wep)[0-9a-f]{2}', self.module):
            self.stem = self.module
        rm = re.match(r'st\d/(r[0-9a-f]{3})', self.rel)
        if rm:
            self.stem = rm.group(1)

    def line_of(self, pos):
        return self.text.count('\n', 0, pos) + 1

    def function_at(self, line):
        """(name, first line) of the function containing `line` (1-based): the last unindented
        definition line above it."""
        for i in range(line - 1, -1, -1):
            l = self.lines[i]
            if l and not l[0].isspace() and not l.startswith(('#', '//', '/*', '*', '}', 'static char', 'extern')):
                m = FUNC_DEF.match(l)
                if m:
                    return m.group(1), i + 1
                if l.endswith(('{', ')')) and '(' in l and '=' not in l.split('(')[0]:
                    n = re.search(r'([\w~:]+)\s*\(', l)
                    if n:
                        return n.group(1), i + 1
        return '?', 1


# archive-expression classifiers; the class decides which archive the index addresses
def classify_arc(arc_expr, f, func_text):
    a = arc_expr.replace(' ', '')
    a = re.sub(r'^\((PlArc|RoomArc|SsArc)\*\)', '', a)
    a = re.sub(r'\((\w+)\)', r'\1', a)     # a macro's parenthesised parameter: EM_ARC(pl, n) -> (pl)->subArc
    if a in ('pG->pPlayer', 'PL_DATA_ADDR', '(PlArc*)PL_DATA_ADDR'):
        return 'player', None
    if a in ('pG->pRoom', 'pGS->pRoom', '(PlArc*)pG->pRoom'):
        return 'room', f.stem
    if a in ('pG->pWep', 'WEP_DATA_ADDR'):
        # the module's own file names the archive; a shared wep/pl_*.cpp routine is linked into
        # several modules (refs consumers map it through WEP_UNITS)
        return 'wep', f.stem if f.module.startswith('wep') else None
    if re.fullmatch(r'(em|pEm|owner|pP|p|e|boss)->subArc|\(\(cEm\*\)\w+\)->subArc|.*pEmCatch->subArc|.*->pEm->subArc|.*pEmCatch\)->subArc', a):
        return 'em', f.stem
    if re.fullmatch(r'(pl|pPL|s|sub|pPl|this|pSub|pS)->subArc|subArc', a):
        # the player's (or partner's) subArc. A module that redirects it (`pl->subArc = em->subArc`,
        # `pl->m_pBoat->subArc`, `door2->subArc`: the grab / ride / door routines, often in a
        # dispatcher that then calls the table routines reading PL_ARC(n)) plays its own archive
        # through it; a file that never assigns it reads the player archive (cPlayer::cPlayer sets
        # subArc = PL_DATA_ADDR, every routine restores subArc = subArc2).
        if re.search(r'subArc\s*=\s*(?!\w*->subArc2)[\w.>()-]*->subArc\b|subArc\s*=\s*arc\b', f.text):
            return 'em', f.stem
        return 'sub', None
    if a.startswith('wk->pCmmn'):
        return 'ss', 'ss_cmmn'
    if a.startswith('wk->pTerm'):
        return 'ss', 'ss_term'
    if a in ('d', 'wk->pPartner', 'SubScreenWk.pPartner'):
        return 'ss', 'ss_ocNNN'
    if a.startswith('(SsArc*)(wk)->pWepDat') or 'pWepDat' in a:
        return 'ss', 'ss_wepNN'
    return None, None


def expand_macro(name, args, f):
    """Local ARC-style macro -> (archive expression, index expression) or None."""
    if name == 'PL_ARC_PTR' or name == 'ROOM_ARC_PTR' or name == 'SS_ARC_PTR':
        return args[0], args[1]
    if name == 'WEP_ARC_PTR':
        return 'pG->pWep', args[0]
    if name == 'SUB_MOT':
        return f'{args[0]}->subArc', args[1]
    if name in f.macros:
        p0, p1, body = f.macros[name]
        body = body.strip()
        b = body.replace(p0, args[0]) if p0 else body
        if p1 and len(args) > 1:
            b = b.replace(p1, args[1])
        m = re.match(r'(\w+)\((.*)\)$', b)
        if m:
            inner = split_args(m.group(2))
            return expand_macro(m.group(1), inner, f)
    if name == 'PL_ARC':   # pl_mod.h: the player archive
        return 'pG->pPlayer', args[0]
    if name == 'WEP_ARC':  # ss_model.cpp
        return '(SsArc*) (wk)->pWepDat', args[1]
    if name in ('SUB_ARC',) and len(args) == 2:   # pl_mod.h SUB_ARC(pl, no)
        return f'{args[0]}->pEm->subArc', args[1]
    return None


def resolve(expr, f, func_text, depth=0):
    """(class, stem, index) or None for a DATA expression."""
    e = expr.strip()
    e = re.sub(r'^\((void|u8|MotionData|RockMotData)\s*\*+\)\s*', '', e).strip()
    m = re.match(r'(\w+)\s*\((.*)\)$', e, re.S)
    if m:
        args = split_args(m.group(2))
        ex = expand_macro(m.group(1), args, f)
        if ex:
            arc, idx = ex
            n = const(idx)
            if n is None:
                # a variable index (loop / table): unresolved
                return None
            cls, stem = classify_arc(arc, f, func_text)
            if cls is None and depth < 3:
                # the archive expression is a local: `arc = em->subArc`
                am = re.search(r'\b' + re.escape(arc.strip()) + r'\s*=\s*([^;]+);', func_text)
                if am:
                    cls, stem = classify_arc(am.group(1), f, func_text)
            if cls is None:
                return None
            return cls, stem, n
        return None
    if re.fullmatch(r'\w+', e) and depth < 4:
        # a local variable: its last assignment in the function before use
        for am in reversed(list(re.finditer(r'\b' + re.escape(e) + r'\s*=\s*([^;=]+);', func_text))):
            r = resolve(am.group(1), f, func_text, depth + 1)
            if r:
                return r
        return None
    return None


_field_cache = {}
_wep_units = {}     # weapon module -> its source files (module_units), for field_sources


def field_sources(expr, f, files):
    """[(class, stem, index)] for a work-field expression: the constant archive entries assigned to
    that field (`w->mot[3] = ARC(0x1BC);`, `w->motIdle = idle;` is not constant) in the files of the
    same module directory."""
    m = re.fullmatch(r'(?:\w+->|\w+\.)?(\w+)(?:\[(\w+)\])?', expr.strip())
    if not m:
        return []
    field, idx = m.group(1), m.group(2)
    key = (f.rel if field in ('m_MotTbl', 'm_MotTbl2') else f.module, field, idx)
    if key in _field_cache:
        return _field_cache[key]
    if idx is not None and const(idx) is None:
        _field_cache[key] = []
        return []
    sel = re.escape(field) + (r'\[\s*(0x[0-9A-Fa-f]+|\d+)\s*\]' if idx is not None else r'()\b')
    # `w->mot[3] = ARC(..);` or the PSet(dst, src) macro of the weapon modules (m_MotTbl slots)
    pat = re.compile(r'(?:->|\.)' + sel + r'\s*=\s*([^;=]+);|PSet\((?:\w+->|\w+\.)?' + sel + r',\s*([^;]+)\);')
    # `WEP_MOT(pl, 0x00, 0x0B);`: a FILL_MACRO of this field, its value expression with the argument
    fill_pats = [(re.compile(r'\b' + name + r'\(\s*[^,()]+,\s*(\w+)\s*,\s*(\w+)\s*\)\s*;'), vparam, expr)
                 for name, (ffield, _, vparam, expr) in FILL_MACROS.items() if ffield == field and idx is not None]
    # the player's motion table is filled by every weapon module (WeaponInitFunc) and pl_ashley.cpp:
    # a generic reader (game/, em*/) sees every module's fill, a weapon routine only its own module's
    # (the shared wep/pl_*.cpp objects are linked into several modules: WEP_UNITS); a work field
    # belongs to its module
    everywhere = field in ('m_MotTbl', 'm_MotTbl2')
    own = set()
    if everywhere and (f.module.startswith('wep') or f.module == 'wep'):
        for mod, units in _wep_units.items():
            if f.rel in units or f.rel.startswith(mod + '/'):
                own.add(mod)
    want = None if idx is None else const(idx)
    out = []
    for g in files:
        if not everywhere and g.module != f.module:
            continue
        if own and g.module not in own and not any(g.rel in _wep_units[m] for m in own):
            continue
        for am in pat.finditer(g.text):
            i = am.group(1) or am.group(3)
            if want is not None and (i is None or const(i) != want):
                continue
            line = g.line_of(am.start())
            func, fline = g.function_at(line)
            func_text = '\n'.join(g.lines[fline - 1:line])
            r = resolve(am.group(2) or am.group(4), g, func_text)
            if r and r + (g.rel, line) not in out:
                out.append(r + (g.rel, line))
        for fp, vparam, expr in fill_pats:
            for am in fp.finditer(g.text):
                if const(am.group(1)) != want:
                    continue
                line = g.line_of(am.start())
                func, fline = g.function_at(line)
                func_text = '\n'.join(g.lines[fline - 1:line])
                r = resolve(re.sub(r'\b' + vparam + r'\b', am.group(2), expr), g, func_text)
                if r and r + (g.rel, line) not in out:
                    out.append(r + (g.rel, line))
    _field_cache[key] = out
    return out


def model_kind(expr, f):
    """What plays the motion: 'player', 'partner' or 'other' (enemy / object / camera)."""
    e = expr.replace(' ', '')
    if e in ('pl', 'pPL', 'pPLS', 'p') or e.startswith(('pl,', 'pPL->', 'pl->')):
        return 'player'
    if e == 'this' and (f.rel.startswith('game/pl_') or f.rel in ('game/player.cpp',) or f.module in ('pl0a', 'pl0d', 'pl06', 'pl02', 'pl03')):
        return 'player'
    if e in ('s', 'sub', 'pSub', 'pEm') and (f.module in ('pl0e', 'pl0f', 'pl11', 'pl14') or 'pl_npc' in f.rel):
        return 'partner'
    if e in ('s', 'sub') and f.module.startswith('em'):
        return 'partner'
    return 'other'


def params_of(f, fline):
    """Parameter names of the function defined at `fline`."""
    sig = f.lines[fline - 1]
    m = re.search(r'\((.*)\)', sig)
    if not m:
        return []
    return [re.sub(r'.*?([A-Za-z_]\w*)\s*(\[\d*\])?$', r'\1', a.strip()) for a in split_args(m.group(1)) if a.strip()]


def scan():
    """Resolves the DATA arguments of every motion-setting call. A wrapper that passes one of its
    own parameters to MotionSetCore (em2bBlendMotSet(m0, ..), cMot3::set, MotSetObj00, cPlayer::motionSet
    ...) is added to the call set and its callers scanned in the next round, until no wrapper is new."""
    files = []
    for dp, dn, fn in os.walk(SRC):
        for name in sorted(fn):
            if name.endswith('.cpp'):
                files.append(File(os.path.join(dp, name)))
    refs = []
    unresolved = []
    wrappers = {}       # short function name -> {param index: 'data' | 'seq'}
    calls = CALL
    seen = set()
    extra = {'MotionSetCore', 'PlRegistMotion', 'SubCharRegistMotion', 'setEvtMotion', 'setGondolaMotion', 'setR11DMotion',
             'motionSet', 'MotionSet'}
    while True:
        new_wrappers = {}
        for f in files:
            for m in calls.finditer(f.text):
                if (f.rel, m.start()) in seen:
                    continue
                seen.add((f.rel, m.start()))
                line = f.line_of(m.start())
                func, fline = f.function_at(line)
                if fline == line:      # the definition itself
                    continue
                args, end = call_args(f.text, m.end())
                if any(a.startswith(('void*', 'void *', 'int ', 'u16 ', 'u8 ', 'DB_MODEL')) for a in args):
                    continue           # a declaration / definition
                func_text = '\n'.join(f.lines[fline - 1:f.line_of(end)])
                kind = next(g for g in m.groups() if g and g not in ('->', '.'))
                model = None
                if kind == 'MotionSetCore':
                    if len(args) < 3:
                        continue
                    model, datas = args[0], [(args[2], args[3] if len(args) > 3 else '0')]
                elif kind == 'motionSet':
                    model = f.text[max(0, m.start() - 40):m.start()].split()[-1] if m.start() else 'this'
                    model = re.sub(r'^.*?([\w\]\[.>()-]+)$', r'\1', model)
                    if len(args) == 6:
                        datas = [(args[0], args[1]), (args[2], args[3])]
                    elif len(args) >= 1:
                        datas = [(args[0], args[4] if len(args) > 4 else '0')]
                    else:
                        continue
                elif kind == 'MotionSet':
                    model = 'camera'
                    datas = [(args[0], '0')] if args else []
                elif kind in ('PlRegistMotion', 'SubCharRegistMotion'):
                    model = 'pPL' if kind == 'PlRegistMotion' else 'partner'
                    datas = [(a, '0') for a in args]
                elif kind in ('setEvtMotion', 'setGondolaMotion', 'setR11DMotion'):
                    model = 'em10'
                    datas = [(a, '0') for a in args]
                elif kind == 'set' and m.group(0).startswith('mot3.'):      # cMot3::set(model, m0, m1, m2, ..): three-way blend
                    model = args[0]
                    datas = [(a, '0') for a in args[1:4]]
                elif kind == 'set0' and m.group(0).startswith('mot3.'):     # cMot3::set0(m, ..)
                    model = 'pl'
                    datas = [(args[0], '0')]
                else:                  # a wrapper found in an earlier round
                    w = wrappers[kind]
                    model = f.text[max(0, m.start() - 40):m.start()].split()[-1] if m.group(2) else (args[0] if args else 'this')
                    model = re.sub(r'^.*?([\w\]\[.>()-]+)$', r'\1', model)
                    datas = []
                    for i, role in sorted(w.items()):
                        if role == 'data' and i < len(args):
                            seq_i = [j for j, r in w.items() if r == 'seq' and j > i]
                            datas.append((args[i], args[seq_i[0]] if seq_i and seq_i[0] < len(args) else '0'))
                params = params_of(f, fline)
                for data, seq in datas:
                    d = data.strip()
                    if d in ('0', 'NULL', 'zero', '(void*) 0', '(void*) NULL'):
                        continue
                    if kind in wrappers and (const(d) is not None or d.startswith(('"', '&', '-')) or d == 'this'):
                        continue       # a same-named method of another class (cFlag.set(1), dmg.set(0, 10))
                    r = resolve(data, f, func_text)
                    if r is None:
                        # a work field (`w->mot[3]`, `w->motIdle`): every assignment of that field in
                        # the module's sources (emNN_set.cpp fills mot[] per model type)
                        for rr in field_sources(d, f, files):
                            refs.append({'class': rr[0], 'stem': rr[1], 'index': rr[2], 'file': f.rel, 'line': line, 'function': func,
                                         'call': kind, 'model': model_kind(model or '', f), 'seq': None, 'via': d,
                                         'fill': f'{rr[3]}:{rr[4]}'})
                        if field_sources(d, f, files):
                            continue
                    if r is None:
                        if kind in wrappers:
                            continue       # a second-order call: only its resolved arguments count
                        if d in params and func != '?':
                            short = func.split('::')[-1]
                            new_wrappers.setdefault(short, {})[params.index(d)] = 'data'
                            s = seq.strip()
                            if s in params:
                                new_wrappers[short][params.index(s)] = 'seq'
                        else:
                            unresolved.append({'file': f.rel, 'line': line, 'function': func, 'call': kind, 'data': data})
                        continue
                    cls, stem, idx = r
                    sq = resolve(seq, f, func_text) if seq and seq.strip() not in ('0', 'NULL', 'zero') else None
                    refs.append({'class': cls, 'stem': stem, 'index': idx, 'file': f.rel, 'line': line, 'function': func,
                                 'call': kind, 'model': model_kind(model or '', f) if kind != 'MotionSet' else 'camera',
                                 'seq': sq[2] if sq else None})
        fresh = {k: v for k, v in new_wrappers.items() if k not in wrappers and k not in extra and WRAPPER_NAME.search(k)}
        if not fresh:
            break
        wrappers.update(fresh)
        names = '|'.join(re.escape(k) for k in sorted(wrappers))
        calls = re.compile(r'\b(MotionSetCore|PlRegistMotion|SubCharRegistMotion|setEvtMotion|setGondolaMotion|setR11DMotion)\s*\('
                           r'|(->|\.)(motionSet|MotionSet)\s*\(|\bmot3\.(set|set0)\s*\(|\b(' + names + r')\s*\(')
    return refs, unresolved, wrappers


def file_tables():
    """FileTbl names (dvd.cpp), the EmFileTbl* and wep_data_* tables (read.cpp) -> stems."""
    dvd = open(os.path.join(SRC, 'game', 'dvd.cpp'), encoding='utf-8').read()
    body = dvd[dvd.index('FileTblEntry FileTbl[] = {'):]
    body = body[:body.index('};')]
    names = re.findall(r'\{"([^"]+)",\s*\d+\}', body)
    read = open(os.path.join(SRC, 'game', 'read.cpp'), encoding='utf-8').read()
    tables = {}
    for m in re.finditer(r'ReadFile (\w+)\[(\d+)\] = \{(.*?)\};', read, re.S):
        ents = re.findall(r'\{\s*(0x[0-9A-Fa-f]+|\d+),\s*(0x[0-9A-Fa-f]+|\d+),\s*(\d+)\s*\}', m.group(3))
        tables[m.group(1)] = [(int(a, 0), int(b, 0), int(c)) for a, b, c in ents]

    def stem_of(file_idx):
        if file_idx == 0:
            return None
        n = names[file_idx]
        return os.path.splitext(os.path.basename(n))[0].lower()
    # ReadPlayerData: pl_type -> the archives of its costumes (`case N:` of the outer switch, `file = X`)
    body = read[read.index('void ReadPlayerData(int type, int costume)\n{'):]
    body = body[:body.index('\n}')]
    players = {}
    cur = None
    for line in body.split('\n'):
        m = re.match(r'    case (\d+):', line)
        if m:
            cur = int(m.group(1))
            players.setdefault(cur, [])
        m = re.search(r'file = (0x[0-9A-Fa-f]+|\d+);', line)
        if m and cur is not None:
            st = stem_of(int(m.group(1), 0))
            if st not in players[cur]:
                players[cur].append(st)
    return names, {k: [stem_of(a) for a, b, c in v] for k, v in tables.items()}, players


def module_units():
    """module -> [source file under src/] from config/G4BE08/modules.py UNITS (a module absent from
    UNITS is the single unit <mod>/<mod>.cpp)."""
    spec = importlib.util.spec_from_file_location('modules', os.path.join(ROOT, 'config', 'G4BE08', 'modules.py'))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    out = {}
    for name, units in mod.UNITS.items():
        files = []
        for u in units:
            unit = u[0]
            shared = u[2] if len(u) > 2 and isinstance(u[2], str) else None
            files.append(shared or unit)
        out[name] = files
    return out


def main():
    units = module_units()
    _wep_units.update({m: fs for m, fs in units.items() if m.startswith('wep')})
    refs, unresolved, wrappers = scan()
    names, tables, players = file_tables()
    # weapon module archives -> the source files linked into them (the shared wep/pl_*.cpp objects)
    wep_files = {m: fs for m, fs in units.items() if m.startswith('wep')}
    with open(OUT, 'w') as o:
        o.write('"""Generated by gen_refs.py from src/ (do not edit): the archive entry every MotionSetCore call\n'
                'plays and the function that plays it, the file tables of read.cpp, the module unit lists.\n\n'
                'REFS: list of dicts {class, stem, index, file, line, function, call, model, seq[, via, fill]}: via = the\n'
                'work field the pointer went through, fill = file:line of the assignment that filled it; class is player /\n'
                'sub / em / wep / room / ss (gen_refs.py docstring), model is player / partner / other / em10 /\n'
                'camera. UNRESOLVED: the calls whose data pointer is not a constant archive entry (event bins,\n'
                'tables walked by index, work fields) with the expression. WRAPPERS: the functions that pass a\n'
                'parameter on to MotionSetCore (parameter index -> data / seq), whose callers were scanned too.\n'
                'FILE_TBL: dvd.cpp FileTbl names.\n'
                'EM_FILES / WEP_FILES: read.cpp EmFileTbl* / wep_data_* as archive stems by enemy id / weapon\n'
                'number per character table. PLAYER_FILES: ReadPlayerData pl_type -> archive stems (costumes).\n'
                'WEP_UNITS: weapon module -> source files (modules.py UNITS).\n"""\n\n')
        o.write('REFS = [\n')
        for r in refs:
            o.write('    ' + repr(r) + ',\n')
        o.write(']\n\nUNRESOLVED = [\n')
        for r in unresolved:
            o.write('    ' + repr(r) + ',\n')
        o.write(']\n\n')
        o.write('WRAPPERS = ' + repr(wrappers) + '\n\n')
        o.write('FILE_TBL = ' + repr(names) + '\n\n')
        o.write('EM_FILES = {\n')
        for k in ('EmFileTbl', 'EmFileTbl_Ada', 'EmFileTbl_Wesker', 'EmFileTbl_Klauser'):
            o.write(f'    {k!r}: {tables[k]!r},\n')
        o.write('}\n\nPLAYER_FILES = ' + repr(players) + '\n\nWEP_FILES = {\n')
        for k in ('wep_data_leon', 'wep_data_ada', 'wep_data_hunk', 'wep_data_wesker', 'wep_data_klauser'):
            o.write(f'    {k!r}: {tables[k]!r},\n')
        o.write('}\n\nWEP_UNITS = {\n')
        for k in sorted(wep_files):
            o.write(f'    {k!r}: {wep_files[k]!r},\n')
        o.write('}\n')
    c = collections.Counter((r['class'], r['model']) for r in refs)
    print(f'{len(refs)} resolved references, {len(unresolved)} unresolved -> {os.path.relpath(OUT, ROOT)}')
    for k in sorted(c):
        print(f'  {k[0]:7} {k[1]:8} {c[k]}')


if __name__ == '__main__':
    main()
