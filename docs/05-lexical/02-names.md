# 5. Lexical Structure

## 5.4 Comments

### 5.4.1 Line Comments

The characters `//` begin a line comment. The comment extends to the end of
the line and is ignored. A comment form is recognized only outside a string
literal. Inside a string's interpolation slot, `/` is an operator token and
`//` and `/*` are not comment forms (Clause 5.9.3); in the literal segments
they are ordinary characters. Block comments do not exist.

### 5.4.2 Documentation Comments

The characters `///` begin a documentation comment. A documentation comment
attaches to the binding on the immediately following line. Consecutive `///`
lines form one document.

A documentation comment MAY appear before a top-level or block-level binding.
It has no effect on compilation and does not suppress a diagnostic. Its
content is free text. Extraction and rendering are toolchain concerns
(Clause 11.3).

```eko
/// Returns a greeting for the given name.
/// The name is interpolated into the result.
greet: (name: String) -> String =
    "hi {name}"
```

## 5.5 Identifiers

### 5.5.1 Syntax

An **identifier** begins with a character that has the Unicode property
XID_Start or is U+005F LOW LINE (`_`). Every following character MUST have the
Unicode property XID_Continue. Non-ASCII identifiers are permitted.

Identifiers are case-sensitive. Two identifiers that differ only in case are
different identifiers.

### 5.5.2 Case Classes

Every identifier belongs to one of two case classes.

A leading `_` is the privacy prefix (Clause 7.3.1). It is not part of the
name's case class; the class is determined by the characters after the prefix.
A name consisting only of `_` is the unit value and wildcard token; it is not
a binding name and has no case class.

| Class | Condition | Used for |
|---|---|---|
| **PascalCase** | the first character after the privacy prefix is in Unicode category `Lu` or `Lt`, and the remainder contains no `_` | type names, constructors, namespace names |
| **snake_case** | every other identifier | value bindings, functions, modules, type variables |

A name that violates the case class required at its position is an error with
code `E-name-case` (Annex B). The required class for each position is
specified in Clause 7.4 and Clause 7.5. A private type is therefore spelled
`_Person`, and a private function `_parse`; the prefix does not change the
class.

```eko
Person: type = [name: String]
_Person: type = [name: String]     // private type; PascalCase after the prefix
show_name: (p: Person) -> String = p.name
_show_name: (p: Person) -> String = p.name  // private function; snake_case
```

## 5.6 Keywords and Contextual Words

### 5.6.1 Reserved Words

The reserved words are exactly:

```text
import   case   if   else
```

A reserved word MUST NOT be used as an identifier.

### 5.6.2 Contextual Words

The following words have a special meaning only in the indicated position and
are otherwise ordinary identifiers.

| Word | Position | Meaning |
|---|---|---|
| `type` | annotation slot after `:` | type declaration (Clause 6.3.5) |
| `ctype` | annotation slot after `:` | C layout record declaration (Clause 6.3.6) |
| `test` | annotation slot after `:` | test declaration (Clause 6.3.7) |
| `extern` | right-hand side beginning a native declaration | extern declaration (Clause 6.3.8) |
| `Pure` | tail of an extern declaration | audited purity guarantee (Clause 10.2.2) |

A contextual word remains legal as a binding name, a record field name, and a
module name. For example, a binding named `type` is legal; `x: type = ...` is a
type declaration only when `type` occupies the annotation slot.
