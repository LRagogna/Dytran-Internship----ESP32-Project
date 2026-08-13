"""Parse the first ten numbered/bulleted items from a Word document.

Public API is compatible with the PDF JT parser:

    jt_name, steps = parse_document("traveler.docx")
    steps = parse("traveler.docx")

The document may use Word's list formatting (numbered or bulleted) or typed
labels such as ``1.``, ``2)``, or ``3:``.  Word bullets do not contain a
number in their paragraph text, so list-formatted paragraphs are assigned
step numbers in document order, from 1 through 10.
"""

from pathlib import Path
import re

from docx import Document
from docx.table import Table
from docx.text.paragraph import Paragraph


MAX_STEPS = 10
TYPED_NUMBER = re.compile(r"^\s*(10|[1-9])\s*[.):\-]\s*(.+?)\s*$")
JT_NAME = re.compile(r"\bJT\s*([A-Z0-9][A-Z0-9-]*)\b", re.I)


def _is_word_list(paragraph):
    """Return True when a paragraph uses Word numbering or bullet metadata."""
    properties = paragraph._p.pPr
    if properties is not None and properties.numPr is not None:
        return True

    # Built-in styles such as "List Number" commonly keep numPr on the style
    # definition instead of copying it onto each paragraph.
    style_properties = paragraph.style._element.pPr
    if style_properties is not None and style_properties.numPr is not None:
        return True

    return (paragraph.style.name or "").lower().startswith("list")


def _paragraphs(document):
    """Yield paragraphs in the document body and in top-level table cells."""
    for item in document.iter_inner_content():
        if isinstance(item, Paragraph):
            yield item
        elif isinstance(item, Table):
            seen_cells = set()
            for row in item.rows:
                for cell in row.cells:
                    # Merged cells can occur more than once in row.cells.
                    cell_id = id(cell._tc)
                    if cell_id in seen_cells:
                        continue
                    seen_cells.add(cell_id)
                    yield from cell.paragraphs


def _clean(text):
    return re.sub(r"\s+", " ", text or "").strip()


def _extract_steps(document):
    steps = {}
    next_list_number = 1

    for paragraph in _paragraphs(document):
        text = _clean(paragraph.text)
        if not text:
            continue

        typed = TYPED_NUMBER.match(text)
        if typed:
            number = int(typed.group(1))
            steps[number] = typed.group(2)
            next_list_number = max(next_list_number, number + 1)
            continue

        if _is_word_list(paragraph) and next_list_number <= MAX_STEPS:
            # A formatted list label lives in numbering.xml, not paragraph.text.
            # Assigning by document order also supports ordinary bullet lists.
            while next_list_number in steps and next_list_number <= MAX_STEPS:
                next_list_number += 1
            if next_list_number <= MAX_STEPS:
                steps[next_list_number] = text
                next_list_number += 1

    return {number: steps[number] for number in sorted(steps) if number <= MAX_STEPS}


def _document_name(document, docx_path):
    for paragraph in _paragraphs(document):
        match = JT_NAME.search(paragraph.text or "")
        if match:
            return "JT" + match.group(1)
    return Path(docx_path).stem


def parse_document(docx_path):
    """Return ``(jt_name, {step_number: description})`` for a .docx file."""
    path = Path(docx_path)
    if path.suffix.lower() != ".docx":
        raise ValueError("The Word JT parser requires a .docx file.")

    document = Document(path)
    return _document_name(document, path), _extract_steps(document)


def parse(docx_path):
    """Return only ``{step_number: description}`` for a .docx file."""
    return parse_document(docx_path)[1]
