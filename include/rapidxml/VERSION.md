# Vendored RapidXML

This is **not** stock RapidXML 1.13. It is the namespace-aware `rapidxml_ns`
variant, renamed back to the `rapidxml` namespace and patched locally.

| Item | Value |
|---|---|
| Base | RapidXML 1.13 (Marcin Kalicinski, 2009) |
| Namespace support | [`svgpp/rapidxml_ns`](https://github.com/svgpp/rapidxml_ns), commit `04674e3` (2017-05-22) |
| Local changes | namespace `rapidxml_ns` → `rapidxml`; `eof_error`; `parse_error` derives from `std::runtime_error`; sign-compare fix in `xmlns_lookup`; `get_value()` allocates its empty string once; MSVC `/permissive` function-pointer typedefs |

## Why this variant and not a maintained fork

`PROGRESS.md` §4 contains the full comparison. In short:

* **`dwd/rapidxml` (FLXML v2)** is the most actively maintained fork, but it is
  a breaking rewrite that requires C++17/20 (`std::string_view`,
  `std::span`) and lives in the `flxml` namespace. Adopting it means rewriting
  `xml2json.hpp`, not upgrading a header.
* **`Fe-Bell/RapidXML` v1.17** is the best 1.13-lineage fork, but it has no
  XML-namespace support (our `qualify_element_names` option depends on it) and
  its "retain whitespace" fix changes the parse result — and therefore the
  JSON output — for indented documents.
* **`timniederhausen/rapidxml`** has the best build setup, but only the MSVC
  `/permissive` fix is relevant to us; it is applied here.
* **`RmbRT/rapidxml`** is a 2017 C++17 `string_view` rewrite; its heap-allocated
  pool offers nothing that the `unique_ptr` document in `xml2json.hpp` does not
  already provide.

Upgrading this file therefore requires re-applying the namespace patch and
re-verifying byte-for-byte output.
