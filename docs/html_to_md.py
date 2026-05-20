#!/usr/bin/env python3
"""html_to_md.py — convert guides.html / guides_en.html → Markdown.

Tailored to the specific structure of these two files (h1–h5, p, ul, ol,
table, pre/code, figure, details, div.control, div.note, span.tag, svg,
etc.). Preserves every id="..." anchor so README links and intra-doc
cross-references keep working.

Usage:
    python html_to_md.py input.html output.md
"""

import html
import re
import sys
from html.parser import HTMLParser


# Tags whose start tag (and everything until matching end tag) is dropped.
DROP_TAGS = {'head', 'style', 'script'}

# Inline tags (do not introduce block breaks).
INLINE_TAGS = {'code', 'b', 'strong', 'i', 'em', 'u', 'a', 'br',
               'span', 'small', 'sub', 'sup'}

# Void / self-closing tags.
VOID_TAGS = {'br', 'hr', 'img', 'meta', 'link', 'input'}


class Node:
    """Minimal DOM node."""
    __slots__ = ('tag', 'attrs', 'children', 'parent')

    def __init__(self, tag, attrs=None, parent=None):
        self.tag = tag
        self.attrs = attrs or {}
        self.children = []
        self.parent = parent


class TreeBuilder(HTMLParser):
    """Build a minimal DOM tree from an HTML string."""

    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.root = Node('root')
        self.current = self.root
        self.drop_depth = 0  # > 0 means we're inside a DROP_TAGS subtree

    def handle_starttag(self, tag, attrs):
        if self.drop_depth > 0:
            if tag in DROP_TAGS:
                self.drop_depth += 1
            return
        if tag in DROP_TAGS:
            self.drop_depth = 1
            return
        attr_dict = {k: (v if v is not None else '') for k, v in attrs}
        node = Node(tag, attr_dict, self.current)
        self.current.children.append(node)
        if tag in VOID_TAGS:
            return
        self.current = node

    def handle_endtag(self, tag):
        if self.drop_depth > 0:
            self.drop_depth -= 1
            return
        # Walk up the stack to find a matching tag (tolerant of mismatches).
        node = self.current
        while node is not None and node.tag != tag:
            node = node.parent
        if node is None:
            return
        self.current = node.parent if node.parent is not None else self.root

    def handle_startendtag(self, tag, attrs):
        if self.drop_depth > 0:
            return
        attr_dict = {k: (v if v is not None else '') for k, v in attrs}
        self.current.children.append(Node(tag, attr_dict, self.current))

    def handle_data(self, data):
        if self.drop_depth > 0:
            return
        if not data:
            return
        if self.current.children and isinstance(self.current.children[-1], str):
            self.current.children[-1] += data
        else:
            self.current.children.append(data)

    def handle_comment(self, data):
        pass  # drop HTML comments


def reconstruct_html(node):
    """Render a Node subtree back to raw HTML (used for SVG / details)."""
    if isinstance(node, str):
        return html.escape(node, quote=False)
    parts = []
    attrs_str = ''
    for k, v in node.attrs.items():
        if v == '':
            attrs_str += f' {k}'
        else:
            v_esc = html.escape(str(v), quote=True)
            attrs_str += f' {k}="{v_esc}"'
    if node.tag in VOID_TAGS:
        return f'<{node.tag}{attrs_str}/>'
    parts.append(f'<{node.tag}{attrs_str}>')
    for c in node.children:
        parts.append(reconstruct_html(c))
    parts.append(f'</{node.tag}>')
    return ''.join(parts)


def collect_pre_text(node, parts):
    """Recursively collect text content for use inside ``` code blocks ```.

    Inline <em> / <b> wrappers (used for code comments in the source HTML)
    are stripped — only their text is kept.
    """
    if isinstance(node, str):
        parts.append(node)
        return
    for c in node.children:
        collect_pre_text(c, parts)


class Converter:
    def __init__(self, link_rewrite=None):
        self.out = []
        # Stack of list types: list of ('ul' | 'ol', counter)
        self.list_stack = []
        # Map of HTML→MD link href rewriting (e.g. guides.html → guides.md)
        self.link_rewrite = link_rewrite or {}

    # ---- entry point ---------------------------------------------------

    def convert(self, root):
        for child in root.children:
            self._visit_block(child)
        return ''.join(self.out)

    # ---- emit helpers --------------------------------------------------

    def emit(self, s):
        self.out.append(s)

    def emit_block(self, s):
        """Emit a block-level chunk, ensuring a blank line after."""
        s = s.rstrip() + '\n\n'
        self.out.append(s)

    # ---- block-level dispatch -----------------------------------------

    def _visit_block(self, node):
        if isinstance(node, str):
            # Stray text between blocks — usually whitespace.
            stripped = node.strip()
            if stripped:
                self.emit(stripped + '\n\n')
            return
        tag = node.tag
        if tag in DROP_TAGS:
            return
        method = getattr(self, f'_b_{tag.replace("-", "_")}', None)
        if method:
            method(node)
        else:
            # Unknown block tag — recurse into children.
            for c in node.children:
                self._visit_block(c)

    def _b_html(self, n): [self._visit_block(c) for c in n.children]
    def _b_body(self, n): [self._visit_block(c) for c in n.children]
    def _b_main(self, n): [self._visit_block(c) for c in n.children]
    def _b_nav(self, n):
        # The HTML <nav> is a sidebar TOC; skip entirely from the MD output
        # because the chapter headings themselves serve as a sufficient TOC,
        # and the sidebar's nested structure doesn't round-trip cleanly.
        return

    def _b_div(self, n):
        cls = n.attrs.get('class', '')
        cls_set = set(cls.split())
        if 'control' in cls_set:
            anchor = n.attrs.get('id')
            if anchor:
                self.emit_block(f'<a id="{anchor}"></a>')
            self._render_mixed(n)
        elif 'note' in cls_set:
            inner = []
            saved = self.out
            self.out = inner
            self._render_mixed(n)
            self.out = saved
            md = ''.join(inner).strip()
            for line in md.split('\n'):
                if line.strip():
                    self.emit('> ' + line + '\n')
                else:
                    self.emit('>\n')
            self.emit('\n')
        elif 'demo-shots' in cls_set or 'ctl-fig' in cls_set or 'toc-grid' in cls_set:
            # Visual layout wrappers. Recurse into children, which are
            # typically <figure>, <img>, or links.
            for c in n.children:
                self._visit_block(c)
        else:
            self._render_mixed(n)

    def _render_mixed(self, parent):
        """Render mixed inline + block content under a container.

        Inline children (strings and INLINE_TAGS) are accumulated into a
        paragraph buffer; when a block child appears, the buffer is flushed
        first, then the block is rendered. Without this, inline text gets
        emitted as a separate block per inline node which fragments
        blockquotes and paragraphs.
        """
        inline_buf = []

        def flush():
            text = ''.join(inline_buf).strip()
            inline_buf.clear()
            if text:
                # Collapse internal whitespace runs but preserve hard breaks.
                text = re.sub(r'[ \t]*\n[ \t]*', '\n', text)
                self.emit_block(text)

        for c in parent.children:
            if isinstance(c, str):
                inline_buf.append(c)
                continue
            tag = c.tag
            if tag in INLINE_TAGS or tag == 'span':
                inline_buf.append(self._render_inline(c))
                continue
            # Block child — flush any pending inline content first.
            flush()
            self._visit_block(c)
        flush()

    def _heading(self, n, level):
        anchor = n.attrs.get('id')
        if anchor:
            self.emit_block(f'<a id="{anchor}"></a>')
        inline = self._render_inline_children(n)
        # Strip pure-anchor inline content
        self.emit_block('#' * level + ' ' + inline.strip())

    def _b_h1(self, n): self._heading(n, 1)
    def _b_h2(self, n): self._heading(n, 2)
    def _b_h3(self, n): self._heading(n, 3)
    def _b_h4(self, n): self._heading(n, 4)
    def _b_h5(self, n): self._heading(n, 5)
    def _b_h6(self, n): self._heading(n, 6)

    def _b_p(self, n):
        anchor = n.attrs.get('id')
        if anchor:
            self.emit_block(f'<a id="{anchor}"></a>')
        inline = self._render_inline_children(n)
        text = inline.strip()
        if text:
            self.emit_block(text)

    def _b_pre(self, n):
        # Code block.
        parts = []
        collect_pre_text(n, parts)
        text = ''.join(parts)
        # Trim a single leading newline (HTML often has <pre>\n<code>).
        if text.startswith('\n'):
            text = text[1:]
        text = text.rstrip()
        self.emit('```\n' + text + '\n```\n\n')

    def _b_ul(self, n):
        self.list_stack.append(['ul', 0])
        for c in n.children:
            if isinstance(c, str):
                continue
            if c.tag == 'li':
                self._render_li(c)
        self.list_stack.pop()
        self.emit('\n')

    def _b_ol(self, n):
        self.list_stack.append(['ol', 0])
        for c in n.children:
            if isinstance(c, str):
                continue
            if c.tag == 'li':
                self._render_li(c)
        self.list_stack.pop()
        self.emit('\n')

    def _render_li(self, n):
        depth = len(self.list_stack) - 1
        indent = '  ' * depth
        kind, counter = self.list_stack[-1]
        if kind == 'ul':
            marker = '- '
        else:
            self.list_stack[-1][1] += 1
            marker = f'{self.list_stack[-1][1]}. '
        # Separate inline content from any nested ul/ol or block children.
        inline_chunks = []
        nested_blocks = []
        for c in n.children:
            if isinstance(c, str):
                inline_chunks.append(c)
            elif c.tag in ('ul', 'ol'):
                nested_blocks.append(c)
            elif c.tag == 'p':
                inline_chunks.append(self._render_inline_children(c) + ' ')
            elif c.tag in INLINE_TAGS or c.tag == 'span':
                inline_chunks.append(self._render_inline(c))
            else:
                inline_chunks.append(self._render_inline(c))
        line = ''.join(inline_chunks).strip()
        # Collapse internal whitespace
        line = re.sub(r'\s+', ' ', line)
        self.emit(f'{indent}{marker}{line}\n')
        # Nested lists rendered with deeper indent.
        for blk in nested_blocks:
            if blk.tag == 'ul':
                self._b_ul(blk)
            else:
                self._b_ol(blk)

    def _b_table(self, n):
        # Build a row matrix first.
        rows = []
        for c in n.children:
            if isinstance(c, str):
                continue
            if c.tag in ('thead', 'tbody', 'tfoot'):
                # Recurse to find <tr> children.
                for cc in c.children:
                    if isinstance(cc, str):
                        continue
                    if cc.tag == 'tr':
                        rows.append(self._render_row(cc))
            elif c.tag == 'tr':
                rows.append(self._render_row(c))
        if not rows:
            return
        # Determine max column count for padding.
        n_cols = max(len(r) for r in rows)
        # Heuristic for header row: first row consists entirely of <th>.
        first_row_is_header = all(is_th for is_th, _ in rows[0])
        if first_row_is_header:
            header_cells = [text for _, text in rows[0]]
            data_rows = rows[1:]
        else:
            # Generate empty header so the table is still valid GFM.
            header_cells = [' '] * n_cols
            data_rows = rows
        while len(header_cells) < n_cols:
            header_cells.append(' ')
        self.emit('\n| ' + ' | '.join(header_cells) + ' |\n')
        self.emit('| ' + ' | '.join(['---'] * n_cols) + ' |\n')
        for row in data_rows:
            cells = [text for _, text in row]
            while len(cells) < n_cols:
                cells.append('')
            self.emit('| ' + ' | '.join(cells) + ' |\n')
        self.emit('\n')

    def _render_row(self, tr_node):
        """Return list of (is_th, cell_md_text) for one <tr>."""
        cells = []
        for c in tr_node.children:
            if isinstance(c, str):
                continue
            if c.tag in ('td', 'th'):
                inner = self._render_inline_children(c).strip()
                # Cell content in a GFM table can't contain bare newlines or |.
                inner = inner.replace('|', '\\|')
                inner = re.sub(r'\s*\n\s*', ' ', inner)
                inner = re.sub(r'\s+', ' ', inner)
                # Handle colspan by repeating empty cells afterward (rare here).
                colspan = int(c.attrs.get('colspan', '1') or '1')
                cells.append((c.tag == 'th', inner))
                for _ in range(colspan - 1):
                    cells.append((c.tag == 'th', ''))
        return cells

    def _b_figure(self, n):
        # A figure can have multiple <img> + a single <figcaption>.
        imgs = []
        caption_md = ''
        svg_parts = []
        for c in n.children:
            if isinstance(c, str):
                continue
            if c.tag == 'img':
                imgs.append((c.attrs.get('src', ''), c.attrs.get('alt', '')))
            elif c.tag == 'figcaption':
                caption_md = self._render_inline_children(c).strip()
            elif c.tag == 'svg':
                svg_parts.append(reconstruct_html(c))
            else:
                # Unknown child — recurse as block.
                self._visit_block(c)
        for src, alt in imgs:
            self.emit(f'![{alt}]({src})\n\n')
        for svg in svg_parts:
            self.emit(svg + '\n\n')
        if caption_md:
            self.emit(f'*{caption_md}*\n\n')

    def _b_figcaption(self, n):
        # Standalone figcaption (rare).
        text = self._render_inline_children(n).strip()
        if text:
            self.emit_block(f'*{text}*')

    def _b_img(self, n):
        src = n.attrs.get('src', '')
        alt = n.attrs.get('alt', '')
        self.emit_block(f'![{alt}]({src})')

    def _b_svg(self, n):
        self.emit(reconstruct_html(n) + '\n\n')

    def _b_details(self, n):
        # Pass through as raw HTML (GitHub Flavored Markdown supports it).
        self.emit('\n' + reconstruct_html(n) + '\n\n')

    def _b_hr(self, n):
        self.emit('---\n\n')

    def _b_br(self, n):
        # Standalone <br> at block level → just emit a blank line.
        self.emit('\n')

    # ---- inline rendering ---------------------------------------------

    def _render_inline_children(self, node):
        parts = []
        for c in node.children:
            parts.append(self._render_inline(c))
        return ''.join(parts)

    def _render_inline(self, node):
        if isinstance(node, str):
            return self._escape_inline_text(node)
        tag = node.tag
        if tag in ('b', 'strong'):
            inner = self._render_inline_children(node)
            return f'**{inner.strip("*") if inner.endswith("*") or inner.startswith("*") else inner}**' if False else f'**{inner}**'
        if tag in ('i', 'em'):
            return f'*{self._render_inline_children(node)}*'
        if tag == 'u':
            return f'<u>{self._render_inline_children(node)}</u>'
        if tag == 'code':
            text = ''.join(self._raw_text(c) for c in node.children)
            if '`' in text:
                return f'`` {text} ``'
            return f'`{text}`'
        if tag == 'a':
            href = node.attrs.get('href', '')
            href = self._rewrite_link(href)
            label = self._render_inline_children(node)
            return f'[{label}]({href})'
        if tag == 'br':
            return '  \n'
        if tag == 'span':
            cls = node.attrs.get('class', '')
            inner = self._render_inline_children(node)
            if 'tag' in cls.split():
                return f' `[{inner.strip()}]`'
            return inner
        if tag == 'small':
            return f'<small>{self._render_inline_children(node)}</small>'
        if tag == 'sub':
            return f'<sub>{self._render_inline_children(node)}</sub>'
        if tag == 'sup':
            return f'<sup>{self._render_inline_children(node)}</sup>'
        if tag in ('img',):
            return f'![{node.attrs.get("alt", "")}]({node.attrs.get("src", "")})'
        # Unknown inline tag — render children only.
        return self._render_inline_children(node)

    def _raw_text(self, node):
        if isinstance(node, str):
            return node
        return ''.join(self._raw_text(c) for c in node.children)

    def _escape_inline_text(self, text):
        # Don't touch newlines (they're preserved by the block layer).
        # Escape characters that would otherwise be interpreted as MD syntax
        # at the start of a line. We deliberately keep this minimal to avoid
        # ugly output; most inline tokens like `*`, `_`, `[` aren't ambiguous
        # within the rendered prose of these docs.
        return text

    def _rewrite_link(self, href):
        for src, dst in self.link_rewrite.items():
            if href == src:
                return dst
        return href


def post_process(md):
    # Collapse 3+ blank lines to 2.
    md = re.sub(r'\n{3,}', '\n\n', md)
    # Trim trailing whitespace per line.
    md = re.sub(r'[ \t]+\n', '\n', md)
    # Single trailing newline at EOF.
    md = md.rstrip() + '\n'
    return md


def convert_file(in_path, out_path, link_rewrite=None):
    with open(in_path, 'r', encoding='utf-8') as f:
        src = f.read()
    builder = TreeBuilder()
    builder.feed(src)
    builder.close()
    conv = Converter(link_rewrite=link_rewrite or {})
    md = conv.convert(builder.root)
    md = post_process(md)
    with open(out_path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(md)
    return len(md)


def main():
    if len(sys.argv) != 3:
        print('Usage: python html_to_md.py input.html output.md', file=sys.stderr)
        sys.exit(1)
    in_path, out_path = sys.argv[1], sys.argv[2]
    # Default link rewrites: HTML cross-link → MD cross-link.
    link_rewrite = {
        'guides.html': 'guides.md',
        'guides_en.html': 'guides_en.md',
    }
    n = convert_file(in_path, out_path, link_rewrite=link_rewrite)
    print(f'Wrote {out_path} ({n} chars)')


if __name__ == '__main__':
    main()
