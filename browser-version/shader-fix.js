/* Amethyst — HLSL pre-pass for the in-browser MilkDrop converter.

   The browser build of the HLSL→GLSL converter mistranslates any binary expression
   whose LEFT operand is itself an unparenthesised binary expression:
       time*2 + bass        →  bool(time*2) && bool(bass)        (wrong, won't compile)
       (time*2) + bass      →  (time*2) + bass                   (correct)
   This file rewrites shader code with explicit parentheses that follow normal operator
   precedence, which keeps the meaning identical and sidesteps the bug. */
(function (root) {
  'use strict';

  const BINOP_PREC = {
    '*': 10, '/': 10, '%': 10,
    '+': 9, '-': 9,
    '<<': 8, '>>': 8,
    '<': 7, '>': 7, '<=': 7, '>=': 7,
    '==': 6, '!=': 6,
    '&': 5, '^': 4, '|': 3, '&&': 2, '||': 1,
  };
  const SEPARATORS = new Set([';', ',', '?', ':', '=', '+=', '-=', '*=', '/=', '%=', '&=', '|=', '^=', '<<=', '>>=']);
  const SEP_KEYWORDS = new Set(['return', 'if', 'else', 'for', 'while', 'do', 'switch', 'case', 'default', 'break', 'continue', 'discard']);
  const OPS = ['<<=', '>>=', '++', '--', '+=', '-=', '*=', '/=', '%=', '&=', '|=', '^=', '==', '!=', '<=', '>=', '&&', '||', '<<', '>>',
    '+', '-', '*', '/', '%', '<', '>', '=', '!', '~', '&', '|', '^', '?', ':', ';', ',', '.', '(', ')', '[', ']', '{', '}'];
  const CAST_TYPE = /^(float|half|int|uint|bool|double|min16float|min10float)([1-4](x[1-4])?)?$/;
  const CLOSE = { '(': ')', '[': ']', '{': '}' };

  function tokenize(src) {
    const toks = [];
    let i = 0, ws = '';
    const n = src.length;
    while (i < n) {
      const c = src[i];
      if (c === ' ' || c === '\t' || c === '\r' || c === '\n') { ws += c; i++; continue; }
      if (c === '/' && src[i + 1] === '/') { while (i < n && src[i] !== '\n') i++; ws += ' '; continue; }
      if (c === '/' && src[i + 1] === '*') { const e = src.indexOf('*/', i + 2); i = e < 0 ? n : e + 2; ws += ' '; continue; }
      if (c === '#' && /(^|\n)[ \t]*$/.test(src.slice(0, i))) {
        let j = i; while (j < n && src[j] !== '\n') j++;
        toks.push({ t: 'pp', v: src.slice(i, j), ws }); ws = ''; i = j; continue;
      }
      let m = /^(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?[fFhHlLuU]?/.exec(src.slice(i, i + 40));
      if (m && (c !== '.' || /\d/.test(src[i + 1] || ''))) { toks.push({ t: 'num', v: m[0], ws }); ws = ''; i += m[0].length; continue; }
      m = /^[A-Za-z_]\w*/.exec(src.slice(i, i + 200));
      if (m) { toks.push({ t: 'id', v: m[0], ws }); ws = ''; i += m[0].length; continue; }
      const op = OPS.find((o) => src.startsWith(o, i));
      if (op) { toks.push({ t: 'op', v: op, ws }); ws = ''; i += op.length; continue; }
      toks.push({ t: 'other', v: c, ws }); ws = ''; i++;
    }
    return { toks, tail: ws };
  }

  // Nest (), [], {} into groups.
  function nest(toks) {
    const rootList = [];
    const stack = [{ list: rootList, open: null }];
    for (const tk of toks) {
      const top = stack[stack.length - 1];
      if (tk.t === 'op' && CLOSE[tk.v]) {
        const g = { t: 'group', open: tk.v, ws: tk.ws, children: [], closeWs: '' };
        top.list.push(g);
        stack.push({ list: g.children, open: tk.v, group: g });
      } else if (tk.t === 'op' && (tk.v === ')' || tk.v === ']' || tk.v === '}')) {
        if (stack.length < 2 || CLOSE[top.open] !== tk.v) throw new Error('unbalanced');
        top.group.closeWs = tk.ws;
        stack.pop();
      } else {
        top.list.push(tk);
      }
    }
    if (stack.length !== 1) throw new Error('unbalanced');
    return rootList;
  }

  const isCast = (g) => g.t === 'group' && g.open === '(' && g.children.length === 1 && g.children[0].t === 'id' && CAST_TYPE.test(g.children[0].v);
  const isSeparator = (tk) => tk.t === 'pp' || (tk.t === 'group' && tk.open === '{') || (tk.t === 'op' && SEPARATORS.has(tk.v)) || (tk.t === 'id' && SEP_KEYWORDS.has(tk.v));

  function processList(list) {
    for (const tk of list) if (tk.t === 'group') tk.children = processList(tk.children);
    const out = [];
    let seg = [];
    const flush = () => { if (seg.length) out.push(...parenthesize(seg)); seg = []; };
    for (const tk of list) {
      if (isSeparator(tk)) { flush(); out.push(tk); } else seg.push(tk);
    }
    flush();
    return out;
  }

  // Split a flat segment into operands and binary operators, then rebuild with explicit parens.
  function parenthesize(seg) {
    const operands = [];
    const ops = [];
    let cur = [];
    let prevEndsOperand = false;
    for (const tk of seg) {
      const isBin = tk.t === 'op' && BINOP_PREC[tk.v] !== undefined;
      if (isBin && prevEndsOperand && cur.length) {
        operands.push(cur); ops.push(tk); cur = [];
        prevEndsOperand = false;
        continue;
      }
      cur.push(tk);
      if (tk.t === 'id' || tk.t === 'num') prevEndsOperand = true;
      else if (tk.t === 'group') prevEndsOperand = !isCast(tk);
      else if (tk.t === 'op' && (tk.v === '++' || tk.v === '--')) prevEndsOperand = prevEndsOperand; // postfix keeps, prefix stays false
      else if (tk.t === 'op' && tk.v === '.') prevEndsOperand = false;
      else if (tk.t === 'other') prevEndsOperand = true;
      else prevEndsOperand = false;
    }
    operands.push(cur);
    if (ops.length < 2 || operands.some((o) => !o.length)) return seg;

    // Precedence climbing over operands/ops (all binary ops are left-associative).
    let pos = 0;
    const parseExpr = (minPrec) => {
      let left = { leaf: operands[pos] };
      while (pos < ops.length && BINOP_PREC[ops[pos].v] >= minPrec) {
        const op = ops[pos];
        const prec = BINOP_PREC[op.v];
        pos++;
        const right = parseExpr(prec + 1);
        left = { op, left, right };
      }
      return left;
    };
    const tree = parseExpr(0);
    const emit = (node, out) => {
      if (node.leaf) { out.push(...node.leaf); return; }
      out.push({ t: 'op', v: '(', ws: '' });
      emit(node.left, out);
      out.push(node.op);
      emit(node.right, out);
      out.push({ t: 'op', v: ')', ws: '' });
    };
    const out = [];
    emit(tree, out);
    // Keep the segment's leading whitespace where it was.
    if (out.length && out[0].ws === '' && seg[0].ws) { out[0] = Object.assign({}, out[0], { ws: seg[0].ws }); seg[0] = Object.assign({}, seg[0], { ws: '' }); }
    return out;
  }

  function stringify(list) {
    let s = '';
    for (const tk of list) {
      if (tk.t === 'group') s += tk.ws + tk.open + stringify(tk.children) + tk.closeWs + CLOSE[tk.open];
      else s += tk.ws + tk.v;
    }
    return s;
  }

  function parenthesizeHLSL(src) {
    try {
      const { toks, tail } = tokenize(src);
      return stringify(processList(nest(toks))) + tail;
    } catch (_) {
      return src; // leave anything we can't parse untouched
    }
  }

  // Rewrite the warp_N= / comp_N= shader lines of a .milk file.
  function fixMilk(text) {
    const lines = String(text).split(/\r?\n/);
    for (const kind of ['warp', 'comp']) {
      const re = new RegExp('^' + kind + '_(\\d+)=(.*)$');
      const found = [];
      lines.forEach((l, idx) => { const m = re.exec(l); if (m) found.push({ idx, n: Number(m[1]), body: m[2] }); });
      if (!found.length) continue;
      const src = found.slice().sort((a, b) => a.n - b.n).map((f) => (f.body[0] === '`' ? f.body.slice(1) : f.body)).join('\n');
      const fixed = parenthesizeHLSL(src).split('\n');
      const first = found[0].idx;
      const drop = new Set(found.map((f) => f.idx));
      const replacement = fixed.map((l, i) => `${kind}_${i + 1}=\`${l}`);
      for (let i = lines.length - 1; i >= 0; i--) if (drop.has(i) && i !== first) lines.splice(i, 1);
      lines.splice(first, 1, ...replacement);
    }
    return lines.join('\n');
  }

  const api = { parenthesizeHLSL, fixMilk };
  if (typeof module === 'object' && module.exports) module.exports = api;
  root.AmethystShaderFix = api;
})(typeof self !== 'undefined' ? self : this);
