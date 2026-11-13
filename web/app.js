// Study's document view: one page that shows whichever tab is active, either
// rendered (the default) or as editable Markdown source. The window's C++
// side owns tabs and files; this page owns rendering, editing and scrolling,
// and the two talk over Qt WebChannel.
import morphdom from 'morphdom';
import { EditorState, Compartment } from '@codemirror/state';
import {
  EditorView, keymap, lineNumbers, highlightActiveLine, highlightActiveLineGutter,
  drawSelection, dropCursor, rectangularSelection, crosshairCursor, highlightSpecialChars,
} from '@codemirror/view';
import { defaultKeymap, history, historyKeymap, indentWithTab } from '@codemirror/commands';
import { searchKeymap, highlightSelectionMatches } from '@codemirror/search';
import { HighlightStyle, syntaxHighlighting, bracketMatching, indentOnInput } from '@codemirror/language';
import { markdown, markdownLanguage } from '@codemirror/lang-markdown';
import { languages } from '@codemirror/language-data';
import { tags as t } from '@lezer/highlight';

const previewEl = document.getElementById('preview');
const content = document.getElementById('content');
const editorEl = document.getElementById('editor');
const themeEl = document.getElementById('theme');

let bridge = null;
const docs = new Map();      // id -> document state
let active = null;           // active document
let colors = {};

// ---------------------------------------------------------------------------
// Rendering: markdown-it in a worker, patched into the page with morphdom.

const worker = new Worker('worker.js');
let renderSeq = 0;
let inFlight = null;          // {seq, doc, text}
let queued = null;            // {doc, text}

const RENDER_SETTINGS = { breaks: false, linkify: true, typographer: false, math: true, mathMacros: {} };

function requestRender(doc) {
  queued = { doc, text: doc.text };
  if (!inFlight) flushRender();
}

function flushRender() {
  if (!queued) return;
  inFlight = { seq: ++renderSeq, doc: queued.doc, text: queued.text };
  queued = null;
  worker.postMessage({ id: inFlight.seq, text: inFlight.text, settings: RENDER_SETTINGS });
}

worker.onmessage = (e) => {
  const done = inFlight;
  inFlight = null;
  if (done && e.data.id === done.seq) {
    done.doc.html = e.data.html;
    done.doc.htmlText = done.text;
    if (done.doc === active && active.mode === 'preview' && !queued) showHtml(done.doc);
  }
  flushRender();
};

let shown = null;   // {doc, html} currently in the preview DOM

function showHtml(doc) {
  const tpl = document.createElement('template');
  tpl.innerHTML = '<div id="content">' + doc.html + '</div>';
  const next = tpl.content.firstElementChild;
  rewriteResourceUrls(next, doc);
  const switching = !shown || shown.doc !== doc;
  shown = { doc, html: doc.html };
  if (switching) {
    content.replaceChildren(...next.childNodes);
  } else {
    morphdom(content, next, {
      childrenOnly: true,
      onBeforeElUpdated(from, to) {
        if (from.isEqualNode(to)) return false;
        if (from.tagName === 'DETAILS' && from.open !== to.hasAttribute('open')) {
          if (from.open) to.setAttribute('open', ''); else to.removeAttribute('open');
        }
        return true;
      },
    });
  }
  invalidateLayout();
  if (doc.fragment) {
    const f = doc.fragment;
    doc.fragment = null;
    if (scrollToFragment(f)) return;
  }
  scrollPreviewToLine(doc.topLine);
}

// ---------------------------------------------------------------------------
// Local files (images, video) load through the app's study://file/ scheme.

const SCHEME_RE = /^[a-z][a-z0-9+.-]*:/i;

function normalizePath(p) {
  const out = [];
  for (const seg of p.split('/')) {
    if (seg === '' || seg === '.') continue;
    if (seg === '..') out.pop(); else out.push(seg);
  }
  return '/' + out.join('/');
}

function resolvePath(ref, doc) {
  let p = ref;
  try { p = decodeURIComponent(ref); } catch {}
  if (/^file:/i.test(ref)) return normalizePath(decodeURIComponent(new URL(ref).pathname));
  if (p.startsWith('/')) {
    // Like VS Code, a root-relative path starts at the opened folder.
    if (doc.root && !p.startsWith(doc.root + '/')) return normalizePath(doc.root + p);
    return normalizePath(p);
  }
  return normalizePath(doc.dir + '/' + p);
}

function fileUrl(absPath) {
  return 'study://file' + absPath.split('/').map(encodeURIComponent).join('/');
}

function resolveResource(u, doc) {
  if (!u || u.startsWith('#') || u.startsWith('//')) return u;
  if (SCHEME_RE.test(u) && !/^file:/i.test(u)) return u;
  const m = /^([^?#]*)([?#].*)?$/.exec(u);
  return fileUrl(resolvePath(m[1], doc)) + (m[2] || '');
}

function rewriteResourceUrls(root, doc) {
  for (const el of root.querySelectorAll('img[src], video[src], audio[src], source[src], video[poster]')) {
    for (const attr of ['src', 'poster']) {
      const v = el.getAttribute(attr);
      if (v !== null) el.setAttribute(attr, resolveResource(v, doc));
    }
    if (el.tagName === 'IMG' && !el.hasAttribute('loading')) el.setAttribute('loading', 'lazy');
  }
}

// ---------------------------------------------------------------------------
// Source line <-> preview offset. Each block carries [data-line, data-line-end);
// tops map to first lines, bottoms to end lines, linear in between.

let anchors = null;
let blocks = null;

function invalidateLayout() { anchors = null; }

function lineHeightPx() {
  return parseFloat(getComputedStyle(content).lineHeight) || 22;
}

function buildAnchors() {
  const base = previewEl.getBoundingClientRect().top - previewEl.scrollTop;
  const pts = [[0, 0]];
  blocks = [];
  for (const el of content.getElementsByClassName('code-line')) {
    const s = +el.dataset.line, e = +el.dataset.lineEnd;
    blocks.push([s, e, el]);
    const r = el.getBoundingClientRect();
    if (r.width === 0 && r.height === 0) continue;
    pts.push([s, r.top - base], [e, r.bottom - base]);
  }
  const lines = active ? active.text.split('\n').length : 1;
  pts.push([lines, content.getBoundingClientRect().bottom - base]);
  pts.sort((a, b) => a[0] - b[0] || a[1] - b[1]);
  const out = [];
  let lastY = -Infinity;
  for (const p of pts) {
    if (p[1] < lastY) continue;
    out.push(p);
    lastY = p[1];
  }
  anchors = out;
  return out;
}

function lineToY(line) {
  const a = anchors || buildAnchors();
  let lo = 0, hi = a.length;
  while (lo < hi) { const mid = (lo + hi) >> 1; if (a[mid][0] <= line) lo = mid + 1; else hi = mid; }
  if (lo >= a.length) { const last = a[a.length - 1]; return last[1] + (line - last[0]) * lineHeightPx(); }
  const p0 = a[Math.max(0, lo - 1)], p1 = a[lo];
  if (p1[0] === p0[0]) return p0[1];
  return p0[1] + (p1[1] - p0[1]) * (line - p0[0]) / (p1[0] - p0[0]);
}

function yToLine(y) {
  const a = anchors || buildAnchors();
  let lo = 0, hi = a.length;
  while (lo < hi) { const mid = (lo + hi) >> 1; if (a[mid][1] <= y) lo = mid + 1; else hi = mid; }
  if (lo >= a.length) { const last = a[a.length - 1]; return last[0] + Math.max(0, y - last[1]) / lineHeightPx(); }
  const p0 = a[Math.max(0, lo - 1)], p1 = a[lo];
  if (p1[1] === p0[1]) return p0[0];
  return p0[0] + (p1[0] - p0[0]) * (y - p0[1]) / (p1[1] - p0[1]);
}

let expectedScroll = null;

function scrollPreviewToLine(line) {
  const target = Math.max(0, lineToY(line || 0));
  if (Math.abs(previewEl.scrollTop - target) < 1) return;
  previewEl.scrollTop = target;
  expectedScroll = previewEl.scrollTop;
}

let reportTimer = 0;
function reportTopLine() {
  if (reportTimer) return;
  reportTimer = setTimeout(() => {
    reportTimer = 0;
    if (active && bridge) bridge.reportTopLine(active.id, active.topLine);
  }, 400);
}

previewEl.addEventListener('scroll', () => {
  if (!active || active.mode !== 'preview') return;
  if (expectedScroll !== null && Math.abs(previewEl.scrollTop - expectedScroll) < 1) {
    expectedScroll = null;
    return;
  }
  expectedScroll = null;
  active.topLine = yToLine(previewEl.scrollTop);
  reportTopLine();
}, { passive: true });

// The window resizing reflows the text: keep the same source line on top.
let resizeRaf = 0;
new ResizeObserver(() => {
  if (resizeRaf) return;
  resizeRaf = requestAnimationFrame(() => {
    resizeRaf = 0;
    invalidateLayout();
    if (active && active.mode === 'preview') scrollPreviewToLine(active.topLine);
  });
}).observe(content);

document.addEventListener('load', (e) => { if (e.target.tagName === 'IMG') invalidateLayout(); }, true);

// ---------------------------------------------------------------------------
// Links and double click

function scrollToFragment(fragment) {
  let id = fragment;
  try { id = decodeURIComponent(fragment); } catch {}
  const lm = /^L(\d+)(?:,\d+)?$/.exec(id);
  if (lm) {
    active.topLine = Math.max(0, +lm[1] - 1);
    scrollPreviewToLine(active.topLine);
    return true;
  }
  const el = document.getElementById(id) || document.getElementById(id.toLowerCase()) ||
    content.querySelector('a[name="' + CSS.escape(id) + '"]');
  if (!el) return false;
  invalidateLayout();
  previewEl.scrollTop += el.getBoundingClientRect().top - previewEl.getBoundingClientRect().top;
  active.topLine = yToLine(previewEl.scrollTop);
  expectedScroll = null;
  reportTopLine();
  return true;
}

content.addEventListener('click', (e) => {
  const a = e.target.closest('a[href]');
  if (!a || !active) return;
  e.preventDefault();
  const href = a.getAttribute('href');
  if (href.startsWith('#')) { scrollToFragment(href.slice(1)); return; }
  if (SCHEME_RE.test(href) && !/^file:/i.test(href)) { bridge.openExternal(href); return; }
  const hash = href.indexOf('#');
  const pathPart = (hash >= 0 ? href.slice(0, hash) : href).split('?')[0];
  const frag = hash >= 0 ? href.slice(hash + 1) : '';
  const path = pathPart ? resolvePath(pathPart, active) : active.path;
  if (path === active.path) { if (frag) scrollToFragment(frag); return; }
  bridge.openPath(path, frag);
});

content.addEventListener('dblclick', (e) => {
  if (!active || e.target.closest('a, input, button, summary')) return;
  const rectTop = previewEl.getBoundingClientRect().top;
  const line = Math.floor(yToLine(previewEl.scrollTop + e.clientY - rectTop));
  bridge.switchMode(active.id, 'edit', line);
});

// ---------------------------------------------------------------------------
// Editor (CodeMirror)

const themeCompartment = new Compartment();
let editorView = null;

function editorTheme() {
  const c = colors;
  const dark = c.mode !== 'light';
  const theme = EditorView.theme({
    '&': { color: c.foreground, backgroundColor: c.background, height: '100%' },
    '.cm-scroller': { fontFamily: 'var(--code-font)', lineHeight: '1.6' },
    '.cm-content': { caretColor: c.accent, padding: '12px 0 40vh' },
    '.cm-cursor, .cm-dropCursor': { borderLeftColor: c.accent, borderLeftWidth: '2px' },
    '&.cm-focused .cm-selectionBackground, .cm-selectionBackground, .cm-content ::selection': {
      backgroundColor: c.selection + ' !important',
    },
    '.cm-activeLine': { backgroundColor: c.lighter_background + '66' },
    '.cm-gutters': { backgroundColor: c.background, color: c.muted, border: 'none' },
    '.cm-activeLineGutter': { backgroundColor: 'transparent', color: c.foreground },
    '.cm-lineNumbers .cm-gutterElement': { padding: '0 12px 0 16px' },
    '.cm-selectionMatch': { backgroundColor: c.lighter_background },
    '&.cm-focused .cm-matchingBracket': { backgroundColor: c.lighter_background, outline: '1px solid ' + c.muted },
    '.cm-panels': { backgroundColor: c.dark_background, color: c.foreground },
    '.cm-panels.cm-panels-bottom': { borderTop: '1px solid ' + c.muted },
    '.cm-searchMatch': { backgroundColor: c.yellow + '44', outline: '1px solid ' + c.yellow + '88' },
    '.cm-searchMatch.cm-searchMatch-selected': { backgroundColor: c.accent + '66' },
    '.cm-textfield': { backgroundColor: c.background, color: c.foreground, border: '1px solid ' + c.muted },
    '.cm-button': { backgroundImage: 'none', backgroundColor: c.lighter_background, color: c.foreground, border: '1px solid ' + c.muted },
  }, { dark });
  const style = HighlightStyle.define([
    { tag: t.heading, color: c.bright_foreground, fontWeight: 'bold' },
    { tag: t.processingInstruction, color: c.red },          // # ** _ ` markers
    { tag: [t.emphasis], fontStyle: 'italic' },
    { tag: [t.strong], fontWeight: 'bold' },
    { tag: t.strikethrough, textDecoration: 'line-through' },
    { tag: [t.link], color: c.blue },
    { tag: [t.url], color: c.cyan, textDecoration: 'underline' },
    { tag: [t.monospace], color: c.green },
    { tag: [t.quote], color: c.dark_foreground, fontStyle: 'italic' },
    { tag: [t.list], color: c.orange },
    { tag: [t.contentSeparator], color: c.muted },
    { tag: [t.comment, t.meta], color: c.dark_foreground, fontStyle: 'italic' },
    { tag: [t.keyword, t.modifier, t.controlKeyword, t.operatorKeyword], color: c.magenta },
    { tag: [t.string, t.special(t.string), t.regexp], color: c.green },
    { tag: [t.number, t.bool, t.null, t.atom], color: c.orange },
    { tag: [t.function(t.variableName), t.function(t.propertyName)], color: c.blue },
    { tag: [t.typeName, t.className, t.namespace], color: c.cyan },
    { tag: [t.propertyName, t.attributeName], color: c.yellow },
    { tag: [t.tagName], color: c.red },
    { tag: [t.variableName, t.labelName], color: c.foreground },
    { tag: [t.operator, t.punctuation, t.bracket], color: c.light_foreground || c.foreground },
    { tag: t.invalid, color: c.red },
  ]);
  return [theme, syntaxHighlighting(style)];
}

function newEditorState(doc, text) {
  return EditorState.create({
    doc: text,
    extensions: [
      lineNumbers(),
      highlightActiveLineGutter(),
      highlightSpecialChars(),
      history(),
      drawSelection(),
      dropCursor(),
      EditorState.allowMultipleSelections.of(true),
      indentOnInput(),
      bracketMatching(),
      rectangularSelection(),
      crosshairCursor(),
      highlightActiveLine(),
      highlightSelectionMatches(),
      EditorView.lineWrapping,
      markdown({ base: markdownLanguage, codeLanguages: languages }),
      themeCompartment.of(editorTheme()),
      keymap.of([
        // Chromium may keep Ctrl+Shift+V (paste as plain text) from reaching
        // the window's shortcut, so the editor asks for the toggle itself.
        { key: 'Ctrl-Shift-v', run: () => { bridge.switchMode(doc.id, 'preview', -1); return true; } },
        ...searchKeymap, ...historyKeymap, ...defaultKeymap, indentWithTab,
      ]),
      EditorView.updateListener.of((u) => {
        if (u.docChanged) scheduleDirtyCheck(doc);
      }),
    ],
  });
}

function ensureEditor() {
  if (!editorView) {
    editorView = new EditorView({ parent: editorEl });
    editorView.scrollDOM.addEventListener('scroll', () => {
      if (active && active.mode === 'edit') {
        active.topLine = editorTopLine();
        reportTopLine();
      }
    }, { passive: true });
  }
  return editorView;
}

function editorTopLine() {
  const v = editorView;
  // Height from the top of the document to the top of the visible area.
  const h = Math.max(0, v.scrollDOM.getBoundingClientRect().top - v.documentTop);
  const block = v.lineBlockAtHeight(h);
  const line = v.state.doc.lineAt(block.from).number - 1;
  const frac = block.height > 0 ? Math.min(1, Math.max(0, (h - block.top) / block.height)) : 0;
  return line + frac;
}

function scrollEditorToLine(line, placeCursor) {
  const v = editorView;
  const n = Math.min(v.state.doc.lines, Math.max(1, Math.floor(line) + 1));
  const pos = v.state.doc.line(n).from;
  const effects = [EditorView.scrollIntoView(pos, { y: 'start', yMargin: 0 })];
  v.dispatch(placeCursor ? { selection: { anchor: pos }, effects } : { effects });
}

const dirtyTimers = new Map();
function scheduleDirtyCheck(doc) {
  if (dirtyTimers.has(doc.id)) return;
  dirtyTimers.set(doc.id, setTimeout(() => {
    dirtyTimers.delete(doc.id);
    const state = doc === active && editorView ? editorView.state : doc.editorState;
    if (!state) return;
    const dirty = state.doc.length !== doc.savedText.length || state.doc.toString() !== doc.savedText;
    if (dirty !== doc.dirty) {
      doc.dirty = dirty;
      bridge.setDirty(doc.id, dirty);
    }
  }, 150));
}

function currentText(doc) {
  if (doc === active && doc.mode === 'edit' && editorView) return editorView.state.doc.toString();
  if (doc.editorState) return doc.editorState.doc.toString();
  return doc.text;
}

// ---------------------------------------------------------------------------
// Showing documents

function stashActive() {
  if (!active) return;
  if (active.mode === 'edit' && editorView) {
    active.topLine = editorTopLine();
    active.editorState = editorView.state;
    active.text = editorView.state.doc.toString();
  }
}

function show(doc, editLine) {
  if (!doc) {
    active = null;
    previewEl.hidden = true;
    editorEl.hidden = true;
    return;
  }
  active = doc;
  if (doc.mode === 'edit') {
    previewEl.hidden = true;
    editorEl.hidden = false;
    const v = ensureEditor();
    if (!doc.editorState) doc.editorState = newEditorState(doc, doc.text);
    v.setState(doc.editorState);
    v.dispatch({ effects: themeCompartment.reconfigure(editorTheme()) });
    const line = editLine >= 0 ? editLine : doc.topLine;
    requestAnimationFrame(() => {
      scrollEditorToLine(line, editLine >= 0);
      v.focus();
    });
  } else {
    editorEl.hidden = true;
    previewEl.hidden = false;
    if (doc.html && doc.htmlText === doc.text) {
      if (!shown || shown.doc !== doc || shown.html !== doc.html) showHtml(doc);
      else scrollPreviewToLine(doc.topLine);
    } else {
      requestRender(doc);
    }
    previewEl.focus({ preventScroll: true });
  }
}

function setMode(doc, mode, line) {
  if (doc === active) stashActive();
  doc.mode = mode;
  if (mode === 'preview' && doc.editorState) doc.text = doc.editorState.doc.toString();
  if (typeof line === 'number' && line >= 0 && mode === 'preview') doc.topLine = line;
  if (doc === active) show(doc, mode === 'edit' && line >= 0 ? line : -1);
}

// ---------------------------------------------------------------------------
// Theme

function applyTheme(c) {
  colors = c || {};
  const v = (k, fb) => colors[k] || fb;
  const dark = colors.mode !== 'light';
  document.documentElement.style.colorScheme = dark ? 'dark' : 'light';
  document.body.classList.toggle('light', !dark);
  const hl = {
    keyword: v('magenta'), string: v('green'), number: v('orange'), comment: v('dark_foreground'),
    title: v('blue'), type: v('cyan'), builtin: v('cyan'), attr: v('yellow'), tag: v('red'),
    literal: v('orange'), meta: v('dark_foreground'), section: v('accent'), regexp: v('bright_cyan', v('cyan')),
    symbol: v('yellow'), addition: v('green'), deletion: v('red'), variable: v('red'), params: v('foreground'),
  };
  themeEl.textContent =
    ':root{' +
    '--bg:' + v('background', '#1a1b26') + ';' +
    '--fg:' + v('foreground', '#c0caf5') + ';' +
    '--heading:' + v('bright_foreground', v('foreground')) + ';' +
    '--accent:' + v('accent', '#7aa2f7') + ';' +
    '--link:' + v('blue', v('accent')) + ';' +
    '--muted:' + v('dark_foreground', '#565f89') + ';' +
    '--border:' + v('muted', '#414868') + ';' +
    '--code-bg:' + v('dark_background', '#13141c') + ';' +
    '--inline-code-bg:' + v('lighter_background', '#24283b') + ';' +
    '--inline-code-fg:' + v('green', '#9ece6a') + ';' +
    '--quote-bg:' + v('dark_background', '#13141c') + ';' +
    '--selection:' + v('selection', '#292e42') + ';' +
    '--error:' + v('red', '#f7768e') + ';' +
    '}' +
    '.hljs-keyword,.hljs-selector-tag,.hljs-doctag{color:' + hl.keyword + '}' +
    '.hljs-string,.hljs-meta .hljs-string{color:' + hl.string + '}' +
    '.hljs-number,.hljs-literal,.hljs-variable.constant_{color:' + hl.number + '}' +
    '.hljs-comment,.hljs-quote{color:' + hl.comment + ';font-style:italic}' +
    '.hljs-title,.hljs-title.function_{color:' + hl.title + '}' +
    '.hljs-title.class_,.hljs-type{color:' + hl.type + '}' +
    '.hljs-built_in{color:' + hl.builtin + '}' +
    '.hljs-attr,.hljs-attribute,.hljs-property{color:' + hl.attr + '}' +
    '.hljs-name,.hljs-selector-id,.hljs-selector-class{color:' + hl.tag + '}' +
    '.hljs-meta{color:' + hl.meta + '}' +
    '.hljs-section{color:' + hl.section + ';font-weight:bold}' +
    '.hljs-regexp,.hljs-char.escape_{color:' + hl.regexp + '}' +
    '.hljs-symbol,.hljs-bullet{color:' + hl.symbol + '}' +
    '.hljs-addition{color:' + hl.addition + '}' +
    '.hljs-deletion{color:' + hl.deletion + '}' +
    '.hljs-variable,.hljs-template-variable{color:' + hl.variable + '}' +
    '.hljs-params{color:' + hl.params + '}';
  if (editorView && active && active.mode === 'edit') {
    editorView.dispatch({ effects: themeCompartment.reconfigure(editorTheme()) });
  }
}

// ---------------------------------------------------------------------------
// Bridge to the window

function docFromMessage(id, path, dir, root, text, mode, topLine, fragment) {
  return {
    id, path, dir, root, text, savedText: text, mode, topLine: topLine || 0,
    fragment: fragment || null, dirty: false, editorState: null, html: null, htmlText: null,
  };
}

function connect(channel) {
  bridge = channel.objects.bridge;
  const theme = channel.objects.theme;

  applyTheme(theme.colors);
  theme.changed.connect(() => applyTheme(theme.colors));

  bridge.documentOpened.connect((id, path, dir, root, text, mode, topLine, fragment) => {
    docs.set(id, docFromMessage(id, path, dir, root, text, mode, topLine, fragment));
  });

  bridge.documentReloaded.connect((id, text) => {
    const doc = docs.get(id);
    if (!doc) return;
    if (doc === active) stashActive();
    doc.text = text;
    doc.savedText = text;
    doc.dirty = false;
    if (doc.editorState) {
      const st = doc.editorState;
      doc.editorState = st.update({ changes: { from: 0, to: st.doc.length, insert: text } }).state;
    }
    if (doc === active) {
      if (doc.mode === 'edit') {
        const head = Math.min(editorView.state.selection.main.head, text.length);
        editorView.dispatch({
          changes: { from: 0, to: editorView.state.doc.length, insert: text },
          selection: { anchor: head },
        });
        doc.editorState = editorView.state;
      } else {
        requestRender(doc);
      }
    }
  });

  bridge.documentRenamed.connect((id, path, dir) => {
    const doc = docs.get(id);
    if (doc) { doc.path = path; doc.dir = dir; }
  });

  bridge.documentClosed.connect((id) => {
    const doc = docs.get(id);
    docs.delete(id);
    if (shown && shown.doc === doc) { shown = null; content.replaceChildren(); }
    if (doc === active) show(null);
  });

  bridge.activated.connect((id) => {
    const doc = docs.get(id);
    if (doc === active) return;
    stashActive();
    show(doc || null, -1);
  });

  bridge.modeRequested.connect((id, mode, line) => {
    const doc = docs.get(id);
    if (doc) setMode(doc, mode, line);
  });

  bridge.textRequested.connect((id) => {
    const doc = docs.get(id);
    if (!doc) return;
    const text = currentText(doc);
    doc.pendingSave = text;
    bridge.saveText(id, text);
  });

  bridge.saved.connect((id) => {
    const doc = docs.get(id);
    if (!doc || doc.pendingSave === undefined) return;
    doc.savedText = doc.pendingSave;
    doc.pendingSave = undefined;
    scheduleDirtyCheck(doc);
  });

  bridge.fragmentRequested.connect((id, fragment) => {
    const doc = docs.get(id);
    if (!doc) return;
    if (doc === active && doc.mode === 'preview' && shown && shown.doc === doc) scrollToFragment(fragment);
    else doc.fragment = fragment;
  });

  bridge.ready();
}

new QWebChannel(qt.webChannelTransport, connect);
