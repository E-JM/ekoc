# 3. Normative References

The following documents are referred to in the text in such a way that some or
all of their content constitutes requirements of this document. For dated
references, only the edition cited applies. For undated references, the latest
edition applies.

| Reference | Subject | Used in |
|---|---|---|
| RFC 2119, *Key words for use in RFCs to Indicate Requirement Levels* | normative keywords | Clause 1.4.1 |
| RFC 8174, *Ambiguity of Uppercase vs Lowercase in RFC 2119 Key Words* | normative keywords | Clause 1.4.1 |
| RFC 3629, *UTF-8, a transformation format of ISO 10646* | source and string encoding | Clauses 5.1, 9.4 |
| The Unicode Standard, Version 15.0.0 | character properties, scalar values, identifier syntax | Clauses 5.6, 9.4 |
| IEEE 754-2019, *IEEE Standard for Floating-Point Arithmetic* | `Float` semantics, `NaN`, rounding | Clauses 9.6, 9.13 |
| RFC 8259, *The JavaScript Object Notation (JSON) Data Interchange Format* | JSON text syntax and escaping | Clause 9.9 |
| ISO 8601-1:2019, *Date and time — Representations for information interchange* | `Time` display | Clause 9.7 |
| ISO/IEC 9899:2018, *Programming languages — C* | C types, C ABI, symbol linkage | Clause 10 |
| The platform C ABI for the target | calling convention, struct layout, symbol decoration | Clause 10.3, 10.6 |

## 3.1 Informative references

The following documents are informative. They record prior art and the design
space from which Eko's rules were selected; they do not form requirements.

- The design record in [archive/](archive/), indexed in Annex E.
- The conformance suite in [../tests/](../tests/), specified in Annex D.
- The works listed in Annex G.
