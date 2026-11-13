// Markdown rendering worker. Runs markdown-it (the same parser VS Code uses),
// highlight.js and KaTeX off the main thread so the preview stays responsive
// while the document is being typed into.
import MarkdownIt from 'markdown-it';
import hljs from 'highlight.js';
import katex from 'katex';
import katexPlugin from '@vscode/markdown-it-katex';
import frontMatter from 'markdown-it-front-matter';

// ---------------------------------------------------------------------------
// Generational caches: every render touches every block, so anything not used
// during the previous render is dropped. Keeps memory bounded without an LRU.
class GenCache {
  constructor() { this.cur = new Map(); this.prev = new Map(); }
  get(key, compute) {
    let v = this.cur.get(key);
    if (v !== undefined) return v;
    v = this.prev.get(key);
    if (v === undefined) v = compute();
    this.cur.set(key, v);
    return v;
  }
  rotate() { this.prev = this.cur; this.cur = new Map(); }
}

const hlCache = new GenCache();
const mathCache = new GenCache();

const LANG_ALIASES = {
  'c#': 'csharp', 'tsx': 'typescript', 'jsonc': 'json', 'json5': 'json',
  'sh-session': 'shell', 'zsh': 'bash', 'console': 'shell', 'py3': 'python',
};

function highlight(code, lang) {
  return hlCache.get(lang + '\u0000' + code, () => {
    const l = LANG_ALIASES[lang.toLowerCase()] || lang.toLowerCase();
    if (!hljs.getLanguage(l)) return null;
    try {
      return hljs.highlight(code, { language: l, ignoreIllegals: true }).value;
    } catch {
      return null;
    }
  });
}

const cachedKatex = {
  renderToString(latex, opts) {
    const r = mathCache.get((opts.displayMode ? 'D' : 'I') + latex, () => {
      try {
        return { html: katex.renderToString(latex, opts) };
      } catch (err) {
        return { err };
      }
    });
    if (r.err) throw r.err;
    return r.html;
  },
};

// ---------------------------------------------------------------------------
// GitHub-compatible heading slugs (same algorithm as VS Code's preview).
function slugify(text) {
  return encodeURI(
    text.trim().toLowerCase()
      .replace(/\s+/g, '-')
      .replace(/[\]\[!/'"#$%&()*+,.:;<=>?@\\^{|}~`。，、；：？！…—·ˉ¨‘’“”々～‖∶＂＇｀｜〃〔〕〈〉《》「」『』．〖〗【】（）［］｛｝]/g, '')
      .replace(/^-+/, '')
      .replace(/-+$/, '')
  );
}

function inlineText(token) {
  if (!token.children) return token.content;
  let out = '';
  for (const c of token.children) {
    if (c.type === 'text' || c.type === 'code_inline' || c.type === 'math_inline') out += c.content;
    else if (c.type === 'image') out += inlineText(c);
  }
  return out;
}

const ATTR_IGNORING_PREFIX = '<p class="katex-block';

function injectLineAttrs(html, token) {
  if (!token.map || !html.startsWith(ATTR_IGNORING_PREFIX)) return html;
  return '<p data-line="' + token.map[0] + '" data-line-end="' + token.map[1] +
    '" class="code-line ' + html.slice('<p class="'.length);
}

function createMd(s) {
  const md = new MarkdownIt({
    html: true,
    breaks: !!s.breaks,
    linkify: !!s.linkify,
    typographer: !!s.typographer,
  });
  md.linkify.set({ fuzzyLink: false });

  // Front matter is parsed (so line numbers stay right) but never rendered.
  md.use(frontMatter, () => {});

  // Task lists: "- [ ] todo" / "- [x] done".
  md.core.ruler.after('inline', 'task_lists', (state) => {
    const toks = state.tokens;
    for (let i = 2; i < toks.length; i++) {
      const t = toks[i];
      if (t.type !== 'inline' || toks[i - 1].type !== 'paragraph_open' || toks[i - 2].type !== 'list_item_open') continue;
      const first = t.children && t.children[0];
      if (!first || first.type !== 'text') continue;
      const m = /^\[([ xX])\](?=\s|$)/.exec(first.content);
      if (!m) continue;
      first.content = first.content.slice(3);
      const cb = new state.Token('html_inline', '', 0);
      cb.content = '<input class="task-list-item-checkbox" type="checkbox" disabled' + (m[1] === ' ' ? '' : ' checked') + '>';
      t.children.unshift(cb);
      const item = toks[i - 2];
      item.attrJoin('class', 'task-list-item');
      for (let j = i - 3; j >= 0; j--) {
        const o = toks[j];
        if (o.level === item.level - 1 && (o.type === 'bullet_list_open' || o.type === 'ordered_list_open')) {
          if (!(o.attrGet('class') || '').includes('contains-task-list')) o.attrJoin('class', 'contains-task-list');
          break;
        }
      }
    }
  });

  // Source map: every block element carries the editor lines it came from.
  // This drives scroll sync, double-click-to-source and the selection marker.
  md.core.ruler.push('source_map', (state) => {
    const counts = new Map();
    const toks = state.tokens;
    for (let i = 0; i < toks.length; i++) {
      const t = toks[i];
      if (t.map && t.nesting >= 0 && t.type !== 'inline') {
        t.attrJoin('class', 'code-line');
        t.attrSet('data-line', String(t.map[0]));
        t.attrSet('data-line-end', String(t.map[1]));
      }
      if (t.type === 'heading_open') {
        const base = slugify(inlineText(toks[i + 1]));
        const n = counts.get(base) || 0;
        counts.set(base, n + 1);
        t.attrSet('id', n ? base + '-' + n : base);
      }
    }
  });

  const escape = md.utils.escapeHtml;
  md.renderer.rules.fence = (tokens, idx, _opts, _env, self) => {
    const t = tokens[idx];
    const info = t.info ? md.utils.unescapeAll(t.info).trim() : '';
    const lang = info.split(/\s+/g)[0];
    const code = t.content.endsWith('\n') ? t.content.slice(0, -1) : t.content;
    const hl = lang ? highlight(code, lang) : null;
    t.attrJoin('class', 'hljs');
    const codeCls = lang ? ' class="language-' + escape(lang) + '"' : '';
    return '<pre' + self.renderAttrs(t) + '><code' + codeCls + '><div>' +
      (hl ?? escape(code)) + '</div></code></pre>\n';
  };
  md.renderer.rules.code_block = (tokens, idx, _opts, _env, self) => {
    const t = tokens[idx];
    t.attrJoin('class', 'hljs');
    return '<pre' + self.renderAttrs(t) + '><code><div>' + escape(t.content.replace(/\n$/, '')) + '</div></code></pre>\n';
  };

  if (s.math) {
    md.use(katexPlugin, {
      katex: cachedKatex,
      enableBareBlocks: true,
      enableFencedBlocks: true,
      globalGroup: true,
      macros: s.mathMacros || {},
      throwOnError: false,
    });
    for (const rule of ['math_block', 'fence']) {
      const orig = md.renderer.rules[rule];
      md.renderer.rules[rule] = (tokens, idx, ...rest) => injectLineAttrs(orig(tokens, idx, ...rest), tokens[idx]);
    }
  }
  return md;
}

let md = null;
let mdKey = '';

self.onmessage = (e) => {
  const { id, text, settings } = e.data;
  const key = JSON.stringify([settings.breaks, settings.linkify, settings.typographer, settings.math, settings.mathMacros]);
  if (key !== mdKey) {
    md = createMd(settings);
    mdKey = key;
  }
  let html;
  try {
    html = md.render(text, {});
  } catch (err) {
    html = '<pre>' + String(err && err.stack || err).replace(/[&<>]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;' })[c]) + '</pre>';
  }
  hlCache.rotate();
  mathCache.rotate();
  self.postMessage({ id, html });
};
