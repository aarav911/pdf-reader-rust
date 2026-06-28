
# `requirements.md`

## 1. Project Overview & Architecture

The goal is to build a minimal, spec-compliant, and structurally correct PDF parser and renderer in Rust. The architecture must decouple data ingestion, object parsing, layout construction, and graphical rendering.

Performance is **not** the primary goal of this milestone; architectural correctness, robust error handling, and a clear data pipeline are.

```
[File/Bytes] -> [Lexer/Tokenizer] -> [Parser] -> [Object Tree / Cross-Ref] -> [Content Stream Parser] -> [Render Pipeline]

```

---

## 2. Low-Level Ingestion & Lexing (The Byte Level)

The foundational layers must handle arbitrary byte access, as PDFs are heavily dependent on byte offsets.

* **Reader Stream:**
* Must abstract over file-backed memory maps (`memmap2`) or a standard heap-allocated byte buffer.
* Must support random-access reading (`seek`) because PDFs are parsed from back to front.


* **Lexer / Tokenizer:**
* Scan raw bytes into primitive PDF tokens.
* **Required Tokens:** Whitespace, Comments (`%`), Booleans (`true`, `false`), Numbers (Integers and Reals), Literal Strings (`(...)`), Hexadecimal Strings (`<...>`), Names (`/Name`), Array Delimiters (`[ ]`), Dictionary Delimiters (`<< >>`), Streams (`stream`, `endstream`), and Indirect Object keywords (`obj`, `endobj`, `R`).
* Must handle PDF-specific whitespace rules (CR, LF, CR+LF, Null byte, Form Feed).



---

## 3. Object Model & Parsing (The Structural Level)

PDF elements are built out of strongly typed objects. You must implement an AST (Abstract Syntax Tree) representing these types.

### 3.1 Primitive Object Representation

Implement a Rust `enum PdfObject` containing:

* `Null`
* `Boolean(bool)`
* `Integer(i64)`
* `Real(f64)`
* `Name(String)` (Store internally as a validated string or an internalized symbol)
* `String(Vec<u8>)` (Must handle both raw ASCII/literal and UTF-16BE byte markers)
* `Array(Vec<PdfObject>)`
* `Dictionary(HashMap<String, PdfObject>)`
* `Stream(Dictionary, Vec<u8>)` (A dictionary pairing data with a raw byte block)
* `IndirectReference { obj_num: u32, gen_num: u16 }`

### 3.2 File Structure & Cross-Reference (XRef) Parsing

To locate objects, the engine must parse the skeleton of the file.

* **Trailer Parsing:** Locate and parse the `startxref` keyword at the very end of the file to find the byte offset of the XRef table.
* **XRef Table Parser:** Parse classic XRef tables containing object numbers, byte offsets, and generation numbers. (Skip compressed XRef streams for the initial version).
* **Object Resolver:** Implement a lazily evaluated lookup mechanism. Given an `IndirectReference`, it looks up the byte offset in the XRef table, jumps to that position, parses the `obj_num gen_num obj ... endobj` sequence, and returns the concrete `PdfObject`.

---

## 4. Document Catalog & Page Tree Navigation

Once objects can be resolved, the engine must traverse the structural hierarchy.

* **Root Catalog Lookup:** Fetch the main catalog dictionary from the trailer (`/Root`).
* **Page Tree Walk:** Locate the `/Pages` dictionary. Recursively traverse the intermediate node dictionaries (`/Type /Pages`) via their `/Kids` arrays until reaching leaf node dictionaries (`/Type /Page`).
* **Page Map Creation:** Build an indexed vector of leaf `IndirectReference` pointers representing the linear page order of the document.

---

## 5. Stream Decompression (Filters)

PDF streams are almost always compressed. You must implement a modular filter architecture to decode stream bytes.

* **Mandatory Filters (Must implement for Initial Version):**
* `FlateDecode`: Standard zlib/deflate decompression (can leverage the `flate2` crate). Must implement the basic **PNG Predictors** (Spatial predictors often used to optimize text/image compression ratios).


* **Optional/Fallback Filters (For Initial Version Stubbing):**
* `ASCIIHexDecode` / `ASCII85Decode`: Simple text-to-binary conversions.
* `LZWDecode`: Return a clean error or stub out using external crates.



---

## 6. Content Stream Parsing & Graphics State

A page's visual appearance is dictated by a sequential stream of operations called a Content Stream (`/Contents`).

* **Operator Parser:** Parse a secondary stream of tokens representing postscript-like graphical operators (e.g., `q`, `Q`, `cm`, `cm`, `Do`, `BT`, `ET`, `Tj`).
* **Graphics State Stack:** Maintain a stack of drawing contexts (`q` pushes, `Q` pops).
* Track the **Current Transformation Matrix (CTM)** as a 3x3 matrix to handle translations, scaling, and rotations.
* Track font size, current font resource link, text matrix ($T_m$), and line widths.



---

## 7. Essential Rendering Features (The Visual Level)

Your rendering target can be a simple image buffer (e.g., using the `tiny-skia` or `raqote` crates for CPU rasterization). The engine must handle these three core categories:

### 7.1 Vector Graphics (Paths)

* **Path Construction:** Support basic path operators: Move to (`m`), Line to (`l`), Cubic Bézier curves (`c`, `v`, `y`), and Rectangle (`re`).
* **Path Painting:** Support Stroke (`S`), Fill (`f`, `F`), and Fill-and-Stroke (`B`).

### 7.2 Text Rendering

PDF text extraction and display is notoriously difficult because PDFs map character codes directly to glyph positions, not necessarily semantic Unicode strings.

* **Font Resource Mapping:** Resolve the `/Font` dictionary found within the page’s `/Resources` entry.
* **Standard 14 Fonts Support:** Hardcode fallback metrics for standard fonts (Helvetica, Times-Roman, Courier) so you can render basic text layout without parsing complex TTF/OTF files immediately.
* **Text Positioning:** Interpret text matrix operators (`BT` begin text, `ET` end text, `Td` translate text position, `Tj` paint text string).
* **Simple Rendering:** Pass glyph shapes or basic string layouts to your chosen 2D rendering canvas based on the computed text positions.

### 7.3 Raster Images

* **Image XObjects:** Parse the `/XObject` dictionary inside page resources.
* **Inlined/Sampled Images:** Detect `/Subtype /Image`, read its `/Width`, `/Height`, and `/BitsPerComponent`, decompress the raw stream data via your filter pipeline, and draw the raw pixel array onto the canvas using the CTM for sizing and placement.

---

## 8. Verification & Test Suite

To confirm compliance without a GUI, your initial version must pass these programmatic validation tests:

* **Structural Dump Test:** Given a sample PDF, print the resolved page count and object tree structural outline to the console without crashing.
* **Text Extraction Regression:** A CLI utility that extracts raw strings from text content streams (`Tj` / `TJ` operators) and saves them to a `.txt` file, matching a known reference output.
* **Image Buffer Render Output:** Rasterize pages directly to standard `.png` files via a simple CLI implementation (`cargo run -- render input.pdf page_1.png`). Visual validation will be done by comparing your generated PNGs against a standard viewer like MuPDF or Poppler.

---

## 9. Explicitly Omitted Features (Out of Scope for Initial Milestone)

To prevent scope creep, the following components are strictly banned from this phase:

* Interactive elements (forms, annotations, hyperlinks).
* Password decryption and security handlers (`/Encrypt`).
* Complex color spaces (CMYK, ICC profiles, Shading patterns) — treat everything as Grayscale or Basic RGB.
* Linearized PDFs ("Fast Web View" optimization hints).
* Font parsing engines (TrueType/CFF font file parsing from scratch) — rely entirely on standard rasterizer system fallbacks or basic character bounding boxes.
* Multi-threading and asynchronous stream decoding.

---

