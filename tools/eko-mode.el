;;; eko-mode.el --- Major mode for the Eko programming language -*- lexical-binding: t; -*-

;; Copyright (C) 2026

;; Version: 0.4.1
;; Package-Requires: ((emacs "25.1"))
;; Keywords: languages

;; This program is free software; you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation, either version 3 of the License, or
;; (at your option) any later version.

;; This program is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.

;; You should have received a copy of the GNU General Public License
;; along with this program.  If not, see <https://www.gnu.org/licenses/>.

;;; Commentary:

;; Major mode for the Eko programming language (files ending in .eko).
;;
;; The language reference is ~/Work/EkoDoc.  Eko is statically typed
;; and immutable: bindings are `name = value' (introducing),
;; `name := value' (rebinding), and `name <- call' (inverted binding);
;; `case' is the branching primitive and `if' is sugar over it;
;; declarations put `type', `ctype', or `test' in the annotation slot;
;; composites use `[...]' at type position and `{...}' at value
;; position; calls are free functions with `receiver.f(...)' as UFCS;
;; arrows carry the world bit (`!->', `?->', or the elided `!', `?').
;;
;; Features:
;;
;; * Font lock: declaration and binding names, reserved words, slot
;;   words, capabilities, prelude free functions (`show', `panic', ...),
;;   type-namespace members (`List.map', `Int.parse'), UFCS calls and
;;   fields, strings with `{...}' interpolation, character literals,
;;   decimal/hex/binary numbers, `//' and `///' comments, and the
;;   world-bit arrows.
;; * Role-aware type faces: a scanner walks the type spans -- annotation
;;   slots, `type'/`ctype' right-hand sides, result types after arrows,
;;   the only places types occur -- and faces PascalCase there as types
;;   and lowercase as italic type variables, while variant heads after
;;   `=' and `|' and every PascalCase outside the spans font as
;;   constructors.  Generic application is juxtaposed (EkoDoc 06-types.md
;;   §6.9.4), so the roles come from position, not spelling.
;; * Indentation (TAB / `indent-region') that understands `case'
;;   blocks, case branches with optional guards (`PATTERN if cond ->'),
;;   `if'/`else' blocks, declaration and callback blocks, `|' union
;;   variants, and continuation lines.
;; * Imenu index of Types / Tests / Functions / Bindings.
;; * `//' comment support via the usual `comment-dwim' (M-;).
;;
;; Indentation rules, informally (see EkoDoc 02-syntax.md §1):
;;
;; * Top level declarations and `case ...:' headers start at
;;   column 0.
;; * A line whose body follows it (`=', `->', lambda/case/if `:')
;;   indents its body one step (`eko-indent-offset').
;; * A case branch `PATTERN -> body' sits one step under the
;;   innermost enclosing `case ...:' header.
;; * A line that closes blocks (it is indented no deeper than an
;;   open block) starts the next item one step under the block
;;   that survived the dedent -- an arm under a case, a statement
;;   under a body.
;; * `|' continuation lines of a union type align after the `='.
;; * Lines starting with a closing bracket outdent to the line that
;;   opened it.
;; * Blank lines follow the code around them (so `RET' lands in the
;;   block you just opened) but `indent-region' keeps them empty;
;;   comment-only lines are deepened to the code below and never
;;   outdented.

;;; Code:

(require 'cl-lib)
(require 'color)
(require 'face-remap)
(require 'subr-x)

(defgroup eko nil
  "Support for the Eko programming language."
  :group 'languages)

(defcustom eko-indent-offset 4
  "Number of columns for one indentation step in `eko-mode'."
  :type 'natnum
  :group 'eko)

(defconst eko--keywords
  '("import" "case" "if" "else")
  "Reserved words (EkoDoc 02-syntax.md §1.2).")

(defconst eko--constants
  '("True" "False")
  "Boolean literals (EkoDoc 08-builtins.md §1).")

(defconst eko--builtin-types
  '("Bool" "Char" "String" "Bytes"
    "Int" "UInt" "Float"
    "I8" "I16" "I32" "I64" "U8" "U16" "U32" "U64" "F32" "F64"
    "Range" "Option" "Result" "Set"
    "Time" "Mono" "Duration" "Json" "ParseError" "IoError" "Never"
    "File" "OpenMode" "Scope" "Task")
  "Prelude types and machine words (EkoDoc 10-prelude.md §3).")

(defconst eko--capabilities
  '("Out" "In" "Fs" "Env" "Args" "Clock" "Rand" "Proc" "Conc")
  "Capability types (EkoDoc 06-effects.md §2.1).")

(defconst eko--builtins
  '("show" "inspect" "panic" "assert" "assert_eq" "assert_ne")
  "Zero-type free functions (EkoDoc 10-prelude.md §1).")

(defconst eko--lower-name-regexp
  "\\_<[[:lower:]_]\\(?:\\sw\\|\\s_\\)*\\_>"
  "Regexp matching a snake_case identifier: bindings and functions.")

(defconst eko--pascal-name-regexp
  "\\_<_?[[:upper:]]\\(?:\\sw\\|\\s_\\)*\\_>"
  "Regexp matching a PascalCase identifier: types, constructors, namespaces.
A leading `_' makes a type module-private (EkoDoc 05-names.md §9).")

(defconst eko--name-regexp
  "\\_<\\(?:\\sw\\|\\s_\\)+\\_>"
  "Regexp matching one complete Eko identifier.")

(defface eko-number-face
  '((t :inherit font-lock-constant-face))
  "Face for numeric literals in Eko."
  :group 'eko)

(defvar eko-number-face 'eko-number-face
  "Symbol naming the `eko-number-face' face.
Font-lock evaluates face symbols as variables, so the name needs a
self-referential binding the way `font-lock-keyword-face' has.")

(defface eko-arrow-face
  '((t :inherit font-lock-keyword-face))
  "Face for the arrows (`->', `<-', `!->', `?->') in Eko."
  :group 'eko)

(defvar eko-arrow-face 'eko-arrow-face
  "Symbol naming the `eko-arrow-face' face.
Bound to itself for font-lock, like `font-lock-keyword-face'.")

(defface eko-interpolation-face
  '((t :inherit font-lock-variable-name-face))
  "Face for a `{expr}' interpolation inside a string."
  :group 'eko)

(defvar eko-interpolation-face 'eko-interpolation-face
  "Symbol naming the `eko-interpolation-face' face.
Bound to itself for font-lock, like `font-lock-keyword-face'.")

(defface eko-constructor-face
  '((t :inherit font-lock-constant-face))
  "Face for variant constructors in Eko (`Run', `Some', `Ok').
PascalCase is a constructor wherever it is not inside a type span:
in patterns, in arm bodies, and as the head of a variant in a `type'
declaration.  Constructors are values, the same family `True' and
`False' already live in."
  :group 'eko)

(defvar eko-constructor-face 'eko-constructor-face
  "Symbol naming the `eko-constructor-face' face.
Bound to itself for font-lock, like `font-lock-keyword-face'.")

(defface eko-typevar-face
  '((t :inherit font-lock-type-face :slant italic))
  "Face for type variables in Eko (`t', `elem' in type position).
The hue stays with the type family and the slant carries the role, so
`Option t' reads as one type applied to a variable, not two types."
  :group 'eko)

(defvar eko-typevar-face 'eko-typevar-face
  "Symbol naming the `eko-typevar-face' face.
Bound to itself for font-lock, like `font-lock-keyword-face'.")

;; Reference palette, measured pixel-by-pixel from the sample
;; screenshot (a custom theme on a Kanagawa base, background
;; #1F1F28).  Booleans and numbers share one dark teal; keywords,
;; arrows, strings, comments, types and definition names each have
;; their own hue; calls and plain variables use the theme's
;; default foreground (#D9D4B8 warm off-white).
;;
;; Those numbers are absolute and the screenshot was dark, so on a
;; light background the same inks wash into the page: that warm
;; off-white sits at 1.4:1 against #FAFAFA, the type tan at 2.4:1,
;; the comment blue at 2.4:1.  `eko--apply-palette' therefore resolves
;; every ink against the background it will be drawn on, and only when
;; that background is light: hue and saturation carry over, saturation
;; gets a lift (`eko-light-chroma-boost') and lightness is walked down
;; until the colour clears a contrast floor.  A dark theme still draws
;; the palette exactly as measured.
(defcustom eko-constant-foreground "#33A6A6"
  "Foreground colour for booleans in Eko buffers.
Nil means follow the theme instead."
  :type '(choice (const :tag "Follow theme" nil) (color :tag "Colour"))
  :group 'eko)

(defcustom eko-number-foreground "#33A6A6"
  "Foreground colour for numeric literals in Eko buffers.
Nil means follow the face's inheritance instead."
  :type '(choice (const :tag "Follow inheritance" nil) (color :tag "Colour"))
  :group 'eko)

(defcustom eko-keyword-foreground "#937DB6"
  "Foreground colour for keywords (`import', `case', `if', ...) in Eko.
Nil means follow the theme instead."
  :type '(choice (const :tag "Follow theme" nil) (color :tag "Colour"))
  :group 'eko)

(defcustom eko-arrow-foreground "#8370A3"
  "Foreground colour for the arrows in Eko buffers.
Nil means follow the face's inheritance instead."
  :type '(choice (const :tag "Follow inheritance" nil) (color :tag "Colour"))
  :group 'eko)

(defcustom eko-string-foreground "#749269"
  "Foreground colour for string literals in Eko buffers.
Nil means follow the theme instead."
  :type '(choice (const :tag "Follow theme" nil) (color :tag "Colour"))
  :group 'eko)

(defcustom eko-comment-foreground "#78AABF"
  "Foreground colour for // comments in Eko buffers.
Nil means follow the theme instead."
  :type '(choice (const :tag "Follow theme" nil) (color :tag "Colour"))
  :group 'eko)

(defcustom eko-type-foreground "#BDA06D"
  "Foreground colour for type and constructor names in Eko buffers.
Nil means follow the theme instead."
  :type '(choice (const :tag "Follow theme" nil) (color :tag "Colour"))
  :group 'eko)

(defcustom eko-function-foreground "#7C9AD6"
  "Foreground colour for definition names in Eko buffers.
Nil means follow the theme instead."
  :type '(choice (const :tag "Follow theme" nil) (color :tag "Colour"))
  :group 'eko)

(defcustom eko-variable-foreground "#D9D4B8"
  "Foreground colour for binding names in Eko buffers.
The reference theme leaves these at its default foreground; set
nil to follow your own theme instead.  On a light background the
theme's own foreground is used whatever this holds, because a
darkened warm off-white reads as olive."
  :type '(choice (const :tag "Follow theme" nil) (color :tag "Colour"))
  :group 'eko)

(defcustom eko-builtin-foreground "#D9D4B8"
  "Foreground colour for builtins (`show', `List.map') in Eko buffers.
The reference theme renders calls in its default foreground; set
nil to follow your own theme instead.  On a light background the
theme's own foreground is used whatever this holds, because a
darkened warm off-white reads as olive."
  :type '(choice (const :tag "Follow theme" nil) (color :tag "Colour"))
  :group 'eko)

(defcustom eko-constructor-foreground "#33A6A6"
  "Foreground colour for variant constructors (`Run', `Some') in Eko.
The reference ink matches the booleans: `True' and `False' are nullary
constructors already, so the whole value-side of PascalCase shares one
teal.  Set nil to follow the face's inheritance instead."
  :type '(choice (const :tag "Follow inheritance" nil) (color :tag "Colour"))
  :group 'eko)

(defcustom eko-typevar-foreground nil
  "Foreground colour for type variables (`t', `elem') in Eko buffers.
Nil keeps the type face's ink: the slant alone marks the variable, and
`Option t' reads as one type family.  Set a colour to pull the
variables out of the type hue."
  :type '(choice (const :tag "Follow type face" nil) (color :tag "Colour"))
  :group 'eko)

;;; Palette adaptation

(defcustom eko-light-contrast-floor 6.0
  "Minimum contrast ratio for Eko inks on a light background.
The reference palette was measured on a dark background; on a light
one its pastels wash into the page, so each colour is walked down the
lightness scale until it clears this ratio.  6:1 sits between WCAG AA
(4.5:1) and AAA (7:1): deep enough to hold its hue, short of inky."
  :type 'number
  :group 'eko)

(defcustom eko-light-dim-contrast-floor 4.5
  "Minimum contrast ratio for recessed Eko inks on a light background.
Comments and doc comments take this floor instead of
`eko-light-contrast-floor'.  They are meant to recede, but they are
still text, so they are held at WCAG AA while the code goes deeper --
which is also what gives the light palette its hierarchy."
  :type 'number
  :group 'eko)

(defcustom eko-light-chroma-boost 1.6
  "Saturation multiplier for Eko syntax inks on a light background.
Darkening alone keeps the palette's measured HSL saturation, and those
pastels still read as grey on white.  Multiplying saturation first,
then walking lightness down to the contrast floor, brings the hue
back.  1.0 keeps the authored saturation."
  :type 'number
  :group 'eko)

(defun eko--hex-rgb (color)
  "Return the (R G B) components of COLOR as integers 0-255, or nil.
Only plain #RRGGBB is understood, which covers both the palette
defaults and the background a GUI frame reports."
  (when (and (stringp color)
             (string-match "\\`#\\([0-9a-fA-F]\\{6\\}\\)\\'" color))
    (let ((rgb (string-to-number (match-string 1 color) 16)))
      (list (ash rgb -16) (logand (ash rgb -8) 255) (logand rgb 255)))))

(defun eko--luminance (color)
  "WCAG relative luminance of COLOR, or nil when COLOR is not #RRGGBB."
  (let ((rgb (eko--hex-rgb color)))
    (when rgb
      (let ((channels (mapcar (lambda (channel)
                                (let ((v (/ channel 255.0)))
                                  (if (<= v 0.03928)
                                      (/ v 12.92)
                                    (expt (/ (+ v 0.055) 1.055) 2.4))))
                              rgb)))
        (apply #'+ (cl-mapcar #'* '(0.2126 0.7152 0.0722) channels))))))

(defun eko--contrast (foreground background)
  "WCAG contrast ratio between FOREGROUND and BACKGROUND, or nil.
Nil means at least one of the two is not #RRGGBB."
  (let ((fg (eko--luminance foreground))
        (bg (eko--luminance background)))
    (when (and fg bg)
      (/ (+ (max fg bg) 0.05) (+ (min fg bg) 0.05)))))

(defun eko--rgb-hex (rgb)
  "Format RGB, a list of floats 0.0-1.0, as #RRGGBB."
  (apply #'format "#%02x%02x%02x"
         (mapcar (lambda (channel)
                   (min 255 (max 0 (round (* 255.0 channel)))))
                 rgb)))

(defun eko--deepen (color background floor boost)
  "COLOR made legible on BACKGROUND while keeping its hue.
Saturation is first multiplied by BOOST (capped at 1.0), then
lightness is lowered in steps of 3% until the WCAG contrast against
BACKGROUND reaches FLOOR.  COLOR comes back unchanged when it already
clears FLOOR, or when either colour is not #RRGGBB -- a terminal frame
reports `unspecified-bg', and guessing there would be worse than
leaving the authored palette alone."
  (let ((contrast (eko--contrast color background))
        (rgb (eko--hex-rgb color)))
    (cond
     ((or (null contrast) (null rgb) (>= contrast floor)) color)
     (t (let* ((hsl (apply #'color-rgb-to-hsl
                           (mapcar (lambda (channel) (/ channel 255.0))
                                   rgb)))
               (hue (nth 0 hsl))
               (saturation (min 1.0 (* (nth 1 hsl) boost)))
               (lightness (nth 2 hsl)))
          (cl-loop for step from 1 to 30
                   for level = (max 0.0 (- lightness (* step 0.03)))
                   for ink = (eko--rgb-hex
                              (color-hsl-to-rgb hue saturation level))
                   when (>= (eko--contrast ink background) floor)
                   return ink
                   finally return "#000000"))))))

(defun eko--background ()
  "Return the frame background as #RRGGBB, or nil when it is not one.
Only a GUI default face reports a colour; a terminal frame answers
`unspecified-bg'."
  (let ((background (ignore-errors (face-attribute 'default :background))))
    (and (eko--hex-rgb background) background)))

(defun eko--foreground ()
  "Return the frame foreground as #RRGGBB, or nil when it is not one."
  (let ((foreground (ignore-errors (face-attribute 'default :foreground))))
    (and (eko--hex-rgb foreground) foreground)))

(defun eko--light-background-p (background)
  "Non-nil when BACKGROUND is a light colour.
The cut sits at mid-grey in relative luminance, so a pale theme lands
on the light branch and a charcoal one on the dark branch."
  (let ((luminance (eko--luminance background)))
    (and luminance (> luminance 0.5))))

(defconst eko--number-face 'eko-number-face
  "Face used for numeric literals.")

(defconst eko--palette
  '((font-lock-constant-face . eko-constant-foreground)
    (eko-number-face . eko-number-foreground)
    (font-lock-keyword-face . eko-keyword-foreground)
    (eko-arrow-face . eko-arrow-foreground)
    (font-lock-string-face . eko-string-foreground)
    (font-lock-comment-face . eko-comment-foreground)
    (font-lock-comment-delimiter-face . eko-comment-foreground)
    (font-lock-doc-face . eko-comment-foreground)
    (font-lock-type-face . eko-type-foreground)
    (font-lock-function-name-face . eko-function-foreground)
    (font-lock-variable-name-face . eko-variable-foreground)
    (eko-interpolation-face . eko-variable-foreground)
    (eko-constructor-face . eko-constructor-foreground)
    (eko-typevar-face . eko-typevar-foreground)
    (font-lock-builtin-face . eko-builtin-foreground))
  "Face and defcustom pairs that make up the reference palette.")

(defconst eko--palette-dim-faces
  '(font-lock-comment-face font-lock-comment-delimiter-face font-lock-doc-face)
  "Palette faces allowed to sit at `eko-light-dim-contrast-floor'.")

(defconst eko--palette-plain-faces
  '(font-lock-variable-name-face eko-interpolation-face font-lock-builtin-face)
  "Palette faces the reference leaves at the theme's own foreground.
On a light background they take that theme's foreground rather than a
deepened warm off-white: a low-chroma warm ink pushed down the
lightness scale turns olive, which is not what `plain binding' means.")

(defun eko--floor (face)
  "Contrast floor for FACE on a light background."
  (if (memq face eko--palette-dim-faces)
      eko-light-dim-contrast-floor
    eko-light-contrast-floor))

(defun eko--light-ink (face color background)
  "Resolve COLOR, the authored ink for FACE, against a light BACKGROUND.
Syntax inks are deepened with `eko-light-chroma-boost' so the hue
comes back; comments keep the dim floor.  Plain faces -- bindings,
builtins, interpolations -- follow the theme's foreground instead."
  (let ((floor (eko--floor face)))
    (if (memq face eko--palette-plain-faces)
        (eko--deepen (or (eko--foreground) color) background floor 1.0)
      (eko--deepen color background floor eko-light-chroma-boost))))

(defvar-local eko--palette-cookies nil
  "Remapping cookies for the palette installed in this buffer.
`face-remap-add-relative' returns one cookie per remapped face; keeping
them lets a theme change replace the remappings instead of stacking a
second set on top of the first.")

(defun eko--apply-palette ()
  "Remap the shared font-lock faces to the reference palette.
Remapping is buffer-local, so other modes keep their theme.  On a light
background each ink is resolved first -- see `eko--light-ink' --
because the palette was measured on a dark one."
  (dolist (cookie eko--palette-cookies)
    (face-remap-remove-relative cookie))
  (setq eko--palette-cookies nil)
  (let* ((background (eko--background))
         (light (and background (eko--light-background-p background))))
    (dolist (spec eko--palette)
      (let ((color (symbol-value (cdr spec))))
        (when color
          (when light
            (setq color (eko--light-ink (car spec) color background)))
          (push (face-remap-add-relative (car spec) :foreground color)
                eko--palette-cookies))))))

(defun eko--refresh-palettes (&rest _)
  "Rebuild the palette in every Eko buffer.
The remappings hold absolute colours resolved against the background
that was current when they were installed, so a theme change has to
reach the buffers that are already open.  Runs from
`enable-theme-functions', which Omarchy fires when it swaps the
day/night theme; call it by hand after editing the palette."
  (interactive)
  (dolist (buffer (buffer-list))
    (with-current-buffer buffer
      (when (derived-mode-p 'eko-mode)
        (eko--apply-palette)))))

(add-hook 'enable-theme-functions #'eko--refresh-palettes)

(defun eko--escaped-p (pos)
  "Non-nil when the character before POS is escaped by a backslash."
  (let ((backslashes 0))
    (save-excursion
      (goto-char pos)
      (while (and (not (bobp)) (eq (char-before) ?\\))
        (setq backslashes (1+ backslashes))
        (backward-char)))
    (oddp backslashes)))

(defun eko--in-double-string-p (pos)
  "Non-nil when POS is inside a double-quoted string literal."
  (let* ((state (syntax-ppss pos))
         (start (nth 8 state)))
    (and (nth 3 state)
         start
         (eq (char-after start) ?\"))))

(defun eko--skip-literal (pos delim)
  "Position after the single-line literal at POS delimited by DELIM.
Escaped characters inside the literal are skipped."
  (save-excursion
    (goto-char (1+ pos))
    (while (and (not (eobp))
                (not (eq (char-after) delim)))
      (if (eq (char-after) ?\\)
          (forward-char (min 2 (- (point-max) (point))))
        (forward-char 1)))
    (min (point-max) (1+ (point)))))

(defun eko--interpolation-end (start)
  "Position after the brace pair opened at START, or nil when unbalanced.
Escaped characters are skipped, braces are counted in pairs, and
nested string or character literals are skipped whole, so braces in
their text do not close the slot."
  (save-excursion
    (goto-char start)
    (let ((depth 0)
          (end nil))
      (while (and (not end) (not (eobp)))
        (let ((char (char-after)))
          (cond
           ((eq char ?\\) (forward-char (min 2 (- (point-max) (point)))))
           ((memq char '(?\" ?'))
            (goto-char (eko--skip-literal (point) char)))
           ((eq char ?{) (setq depth (1+ depth)) (forward-char 1))
           ((eq char ?}) (setq depth (1- depth)) (forward-char 1)
            (when (zerop depth) (setq end (point))))
           (t (forward-char 1)))))
      end)))

(defun eko--unicode-escape-brace-p (pos)
  "Non-nil when the `{' at POS belongs to a \\u{...} escape.
The backslash before the `u' must itself be unescaped, so `\\u{...}'
is a literal backslash followed by `u' and an interpolation slot."
  (and (> pos (1+ (point-min)))
       (eq (char-before pos) ?u)
       (eq (char-before (1- pos)) ?\\)
       (eko--escaped-p (1- pos))))

(defun eko--match-interpolation (limit)
  "Font-lock matcher for a `{expr}' interpolation up to LIMIT.
Only a `{' that opens a slot in a double-quoted string starts a match;
the whole `{...}' receives `eko-interpolation-face', so the expression
stays visually distinct from the surrounding string.  `syntax-ppss'
can move point, so the state query runs inside `save-excursion' while
the search keeps moving forward."
  (let (start)
    (while (and (not start) (re-search-forward "{" limit t))
      (let ((pos (match-beginning 0)))
        (when (and (not (eko--escaped-p pos))
                   (not (eko--unicode-escape-brace-p pos))
                   (save-excursion (eko--in-double-string-p pos)))
          (setq start pos))))
    (when start
      (let ((end (eko--interpolation-end start)))
        (when end
          (set-match-data (list start end start end))
          (goto-char end)
          t)))))

;;;; Type spans

;; Types live only in heads: the annotation slot after `name:' -- on
;; declarations, bindings and parameters -- the right-hand side of a
;; `type' or `ctype' declaration, and result types after arrows.
;; Bodies hold constructors only, and generic application is
;; juxtaposed (EkoDoc 06-types.md §6.9.4: `AppliedType := TypeAtom
;; {TypeAtom}'), so spelling alone cannot tell `Option String' from
;; `Some 5'.  A small forward scanner walks each span; font-lock asks
;; `eko--match-type-word' for the next identifier and faces it by role:
;;
;;   in a span, PascalCase                -> font-lock-type-face
;;   in a span, lowercase                 -> eko-typevar-face
;;   in a `type =', PascalCase after `='/`|' -> eko-constructor-face
;;   everywhere else, PascalCase          -> eko-constructor-face
;;
;; Lowercase names followed by `:' are parameter or field names and
;; stay unfaced; `_` is the unit type and keeps its constant face; a
;; lowercase name before `.' is a module qualifier (`person.Person').

;; Dynamically bound by `font-lock-default-fontify-region' around the
;; extension and keyword passes.
(defvar font-lock-beg)
(defvar font-lock-end)

(defvar-local eko--span-kind nil
  "Kind of the span `eko--span-advance' is walking, or nil when outside.
`head' marks an annotation slot that ends at a depth-0 `=' (or at
`<-', or its newline); `typedef' marks a `type'/`ctype' right-hand
side, which runs to its closing bracket or to the block's dedent.
Reset by `eko--extend-region' at the start of every fontify pass.")

(defvar-local eko--span-depth 0
  "Bracket depth reached inside the span `eko--span-advance' is walking.")

(defvar-local eko--span-head-p nil
  "Non-nil when the next depth-0 PascalCase name of a typedef is a variant head.
Set after `=' and after every depth-0 `|', cleared by the name that
claims it: in `Run Config | Help | Fail String' the heads are
constructors while `Config' and `String' are payload types.")

(defvar-local eko--span-bracketed-p nil
  "Non-nil when the typedef's right-hand side opened its own bracket.
A record or tuple RHS (`Token: type = [tok: Tok, ...]') ends when a
closer brings the depth back to 0; a variant list (`Run Config | Help')
has no bracket of its own, so its payload groups' closers -- `(Option
Expr)', `[BinOp, Expr, Expr]' -- only unwind the depth they opened, and
the block ends at the dedent instead.")

(defconst eko--annotation-regexp
  (concat "^[ \t]*\\(?:\\(?:" eko--pascal-name-regexp "\\)\\.\\)?"
          "\\(" eko--name-regexp "\\)[ \t]*:")
  "A line opening with a declaration or binding head: `Name: type =',
`List.map: (xs: []t) ...', `name: Int = ...'.  Case headers (`case x:')
do not match -- the colon must follow the line's first name directly --
and candidates inside any open bracket are rejected by
`eko--span-find', which keeps record values (`{title: t, ...}'),
record patterns and callback parameter lists out of type position.")

(defun eko--span-at (colon)
  "Classify the annotation whose colon sits at COLON.
Return (KIND . START) -- `typedef' with the position after the `=' of
a `type' or `ctype' declaration, or `head' with the position after the
colon -- or nil when the slot opens no span (`test' declarations,
`Name: type' with no `=' on its line)."
  (save-excursion
    (goto-char (1+ colon))
    (unless (eq (char-after) ?=)         ; `name :=' rebinds, no annotation
      (skip-chars-forward " \t")
      (cond
       ((looking-at "\\(?:ctype\\|type\\)\\_>")
        (goto-char (match-end 0))
        (skip-chars-forward " \t")
        (and (eq (char-after) ?=)
             (progn
               (forward-char 1)
               (skip-chars-forward " \t")
               (cons 'typedef (point)))))
       ((looking-at "test\\_>") nil)
       (t (cons 'head (point)))))))

(defun eko--span-next-indent (pos)
  "Indentation of the first code line after POS, or nil when none.
Blank lines and comment-only lines are transparent whatever their
column -- they neither continue nor close a typedef block, matching
how `eko--comment-indent' treats them."
  (save-excursion
    (goto-char pos)
    (forward-line 1)
    (while (and (not (eobp))
                (string-match-p "\\`[ \t]*\\(?://.*\\)?\\'"
                                (buffer-substring-no-properties
                                 (line-beginning-position)
                                 (line-end-position))))
      (forward-line 1))
    (unless (eobp) (current-indentation))))

(defun eko--span-advance (limit)
  "Walk the active span from point, up to LIMIT.
Move point past the next identifier and return (ROLE BEG END) -- ROLE
being `type', `typevar' or `ctor' -- or nil when the span ended first.
The walk is token-wise: newlines continue typedef blocks over indented
lines and end head spans at depth 0; `[]' is the list-type prefix and
opens no group.  A span closes at a depth-0 `=', `<-' or `;' and at a
value brace; a typedef's bracketed RHS also closes at its own `]',
while a variant list runs to the dedent.  Comments, strings,
characters and numbers are skipped whole."
  (let (hit done)
    (while (and (null hit) (null done) (< (point) limit))
      (let ((ch (char-after)))
        (cond
         ((eq ch ?\n)
          (if (eq eko--span-kind 'head)
              (if (zerop eko--span-depth)
                  (setq eko--span-kind nil done t) ; a head ends with its line
                (forward-line 1))                  ; wrapped head line
            (let ((indent (eko--span-next-indent (point))))
              (if (or (null indent) (zerop indent))
                  (setq eko--span-kind nil done t) ; block dedented, or EOF
                (forward-line 1)))))
         ((and (eq ch ?/) (eq (char-after (1+ (point))) ?/))
          (end-of-line))
         ((memq ch '(?\" ?'))
          (goto-char (eko--skip-literal (point) ch)))
         ((and (eq ch ?{) (zerop eko--span-depth))
          (setq eko--span-kind nil done t))
         ((eq ch ?\[)
          (if (eq (char-after (1+ (point))) ?\])
              (forward-char 2)                 ; `[]t' list prefix, no group
            (setq eko--span-depth (1+ eko--span-depth))
            (forward-char 1)))
         ((eq ch ?\()
          (setq eko--span-depth (1+ eko--span-depth))
          (forward-char 1))
         ((memq ch '(?\) ?\] ?\}))
          (setq eko--span-depth (1- eko--span-depth))
          (cond
           ((and (eq eko--span-kind 'typedef)
                 (zerop eko--span-depth)
                 eko--span-bracketed-p)
            (forward-char 1)                   ; the RHS's own `]' closes it
            (setq eko--span-kind nil done t))
           ((< eko--span-depth 0)              ; not ours; leave it alone
            (setq eko--span-kind nil done t))
           (t (forward-char 1))))              ; payload `)' or nested group
         ((eq ch ?=)
          (if (and (zerop eko--span-depth)
                   (not (memq (char-before) '(?= ?! ?< ?> ?:)))
                   (not (eq (char-after (1+ (point))) ?=)))
              (setq eko--span-kind nil done t)   ; the body starts after `='
            (forward-char 1)))
         ((and (eq ch ?<) (eq (char-after (1+ (point))) ?-)
               (zerop eko--span-depth))
          (setq eko--span-kind nil done t))      ; `name: T <- call'
         ((eq ch ?\;)
          (setq eko--span-kind nil done t))
         ((eq ch ?\|)
          (when (and (eq eko--span-kind 'typedef) (zerop eko--span-depth))
            (setq eko--span-head-p t))
          (forward-char 1))
         ((and (>= ch ?0) (<= ch ?9))
          (skip-syntax-forward "w_"))
         ((memq (char-syntax ch) '(?w ?_))
          (let ((beg (point)))
            (skip-syntax-forward "w_")
            (let ((name (buffer-substring-no-properties beg (point))))
              (cond
               ((string-match-p "\\`_?[[:upper:]]" name)
                (setq hit (list
                           (if (and (eq eko--span-kind 'typedef)
                                    (zerop eko--span-depth)
                                    eko--span-head-p)
                               'ctor 'type)
                           beg (point))
                     eko--span-head-p nil))
               ((string-equal name "_"))         ; unit type: constant rule
               ((looking-at "[ \t]*:"))          ; parameter or field name
               ((eq (char-after) ?\.))           ; `person.Person' qualifier
               (t (setq hit (list 'typevar beg (point))))))))
         (t (forward-char 1)))))
    hit))

(defun eko--span-find (limit)
  "Enter the next type span between point and LIMIT.
Return non-nil when one was entered, with point at its first token;
otherwise park at LIMIT and return nil.  Candidates whose name is a
reserved word (`else:') or whose colon opens a rebind (`name :='), or
that sit inside any open bracket, are skipped."
  (let (hit)
    (while (and (null hit)
                (re-search-forward eko--annotation-regexp limit t))
      (let* ((name (match-string-no-properties 1))
             (colon (1- (match-end 0)))
             ;; syntax-ppss may move point when asked for an earlier
             ;; position; the guard below then must not upset the
             ;; search, and the regexp is line-anchored besides.
             (state (save-excursion (syntax-ppss colon))))
        (unless (or (member name eko--keywords)
                    (eq (char-after (1+ colon)) ?=)
                    (nth 3 state)
                    (nth 1 state))
          (let ((span (eko--span-at colon)))
            (when span
              (setq eko--span-kind (car span)
                    eko--span-bracketed-p nil
                    eko--span-depth 0
                    eko--span-head-p (eq (car span) 'typedef)
                    hit t)
              (goto-char (cdr span))
              ;; A record or tuple RHS opens its own bracket: consume it
              ;; here so the scan starts inside, and the matching `]' --
              ;; the one that brings the depth back to 0 -- is the one
              ;; that ends the span.
              (when (and (eq (car span) 'typedef) (eq (char-after) ?\[))
                (setq eko--span-bracketed-p t
                      eko--span-depth 1)
                (forward-char 1)))))))
    (unless hit (goto-char limit))
    hit))

(defun eko--match-type-word (limit)
  "Font-lock matcher for identifiers inside type spans, up to LIMIT.
Font lock calls this repeatedly; each call yields one identifier's
match with its role in a group the keyword's highlighters face (group
1 types, group 2 type variables, group 3 variant heads), then leaves
point after it.  Outside a span the search continues to the next
annotation line, so one call may cross several declarations."
  (let (hit)
    (while (and (null hit) (< (point) limit))
      (if eko--span-kind
          (setq hit (eko--span-advance limit))
        (eko--span-find limit)))
    (when hit
      (let* ((beg (nth 1 hit))
             (end (nth 2 hit))
             (groups (cond
                      ((eq (car hit) 'type)
                       (list beg end nil nil nil nil))
                      ((eq (car hit) 'typevar)
                       (list nil nil beg end nil nil))
                      (t (list nil nil nil nil beg end)))))
        (set-match-data (append (list beg end) groups))
        t))))

(defun eko--extend-region ()
  "Widen `font-lock-beg'/`font-lock-end' to whole top-level declarations.
Spans are found from the declaration line that opens them, so a
jit-lock chunk starting inside one would miss its head; the widened
region re-derives every span from its `name:' line.  Also resets the
span state, which each pass rebuilds from the widened region.  Return
non-nil when the region grew."
  (setq eko--span-kind nil
        eko--span-depth 0
        eko--span-head-p nil
        eko--span-bracketed-p nil)
  (let ((beg font-lock-beg)
        (end font-lock-end)
        changed)
    (save-excursion
      ;; Back to the nearest column-0 code line (the declaration head);
      ;; blank lines are walked past, column-0 comments are boundaries.
      (goto-char beg)
      (beginning-of-line)
      (while (and (not (bobp))
                  (or (not (zerop (current-indentation)))
                      (looking-at "[ \t]*\\'")))
        (forward-line -1))
      (setq beg (point))
      ;; Forward past the declaration's block, unless the end already
      ;; sits on a boundary line, in which case leave it be so the
      ;; extension settles instead of growing one declaration per pass.
      (goto-char end)
      (beginning-of-line)
      (when (or (not (zerop (current-indentation)))
                (looking-at "[ \t]*\\'"))
        (forward-line 1)
        (while (and (not (eobp))
                    (or (not (zerop (current-indentation)))
                        (looking-at "[ \t]*\\'")))
          (forward-line 1)))
      (setq end (line-beginning-position)))
    (when (< beg font-lock-beg)
      (setq font-lock-beg beg changed t))
    (when (> end font-lock-end)
      (setq font-lock-end end changed t))
    changed))

(defvar eko-mode-font-lock-keywords
  (list
   ;; Documentation comments ------------------------------------
   '("^[ \t]*///+.*$" 0 font-lock-doc-face t)

   ;; Reserved words --------------------------------------------
   ;; Earlier keywords win, so the reserved words are claimed before
   ;; the declaration and binding patterns below: `else:' is a block
   ;; head (EkoDoc 06-syntax/05-case-patterns.md §6.7.4), not the
   ;; annotated binding `name: type' those patterns describe.
   (cons (regexp-opt eko--keywords 'symbols) 'font-lock-keyword-face)

   ;; Declarations ----------------------------------------------
   ;; `Name: type ...' / `Name: ctype ...' declare a type.
   (list (concat "^[ \t]*\\(" eko--name-regexp
                 "\\)[ \t]*:[ \t]*\\(?:ctype\\|type\\)\\_>")
         '(1 font-lock-type-face))
   ;; `name: test = ...' declares a test.
   (list (concat "^[ \t]*\\(" eko--name-regexp
                 "\\)[ \t]*:[ \t]*test\\_>")
         '(1 font-lock-function-name-face))
   ;; `name: (params) ...' declares a function.
   (list (concat "^[ \t]*\\(" eko--name-regexp
                 "\\)[ \t]*:[ \t]*(")
         '(1 font-lock-function-name-face))
   ;; `Type.member: ...' declares a type-namespace member.
   (list (concat "^[ \t]*\\(" eko--pascal-name-regexp
                 "\\)\\.\\(" eko--lower-name-regexp
                 "\\)[ \t]*:")
         '(1 font-lock-type-face)
         '(2 font-lock-function-name-face))
   ;; `name := ...' and `name: ...' introduce bindings.
   (list (concat "^[ \t]*\\(" eko--lower-name-regexp
                 "\\)[ \t]*:=")
         '(1 font-lock-variable-name-face))
   (list (concat "^[ \t]*\\(" eko--lower-name-regexp
                 "\\)[ \t]*<-")
         '(1 font-lock-variable-name-face))
   ;; Tuple destructuring: `(a, b) = value` and `(p, i) <- call`.
   '("^[ \t]*(\\([^)]*\\))[ \t]*=[^=]" (1 font-lock-variable-name-face))
   '("^[ \t]*(\\([^)]*\\))[ \t]*<-" (1 font-lock-variable-name-face))
   (list (concat "^[ \t]*\\(" eko--lower-name-regexp
                 "\\)[ \t]*:")
         '(1 font-lock-variable-name-face))

   ;; Slot words and extern declarations ------------------------
   (list ":[ \t]*\\(type\\|ctype\\|test\\)\\_>"
         '(1 font-lock-keyword-face))
   (list "[=][ \t]*\\(extern\\)\\_>"
         '(1 font-lock-keyword-face))
   (list "\\(?:extern[ \t]*\"[^\"]*\"[ \t]+\\)\\(Pure\\)\\_>"
         '(1 font-lock-keyword-face))

   ;; Type namespaces and members -------------------------------
   ;; `List.map', `Int.parse', `Todo.show': both halves stay visible.
   (list (concat "\\(" eko--pascal-name-regexp
                 "\\)\\.\\(" eko--lower-name-regexp "\\)")
         '(1 font-lock-type-face)
         '(2 font-lock-builtin-face))
   ;; `C.Ptr.Void', `C.Size', `C.In' (11-ffi.md §3).
   '("\\_<C\\.\\(?:Ptr\\.Void\\|Size\\|In\\)\\_>"
     0 font-lock-type-face)

   ;; Imports: `import csv/lex' (05-names.md §6) ----------------
   (list "\\(\\<import\\>\\)[ \t]+\\([[:lower:]_][[:alnum:]_]*\\(?:/[[:lower:]_][[:alnum:]_]*\\)*\\)"
         '(1 font-lock-keyword-face)
         '(2 font-lock-builtin-face))

   ;; Type spans -------------------------------------------------
   ;; Types live only in heads -- annotation slots, `type'/`ctype'
   ;; right-hand sides, result types after arrows -- while bodies hold
   ;; constructors only; generic application is juxtaposed, so spelling
   ;; alone cannot tell `Option String' from `Some 5'.  One scanner
   ;; matcher yields each span identifier with its role in one of three
   ;; groups; laxmatch lets the other two stay empty (see the "Type
   ;; spans" section).
   (list 'eko--match-type-word
         '(1 font-lock-type-face nil t)
         '(2 eko-typevar-face nil t)
         '(3 eko-constructor-face nil t))

   ;; Prelude names --------------------------------------------
   (cons (regexp-opt eko--constants 'symbols) 'font-lock-constant-face)
   (cons (regexp-opt eko--builtins 'symbols) 'font-lock-builtin-face)
   (cons (regexp-opt eko--capabilities 'symbols) 'font-lock-type-face)
   (cons (regexp-opt eko--builtin-types 'symbols) 'font-lock-type-face)
   ;; PascalCase outside the spans above is a constructor: `Some' in a
   ;; pattern, `Ok' in an arm body.  Inside spans the matcher has
   ;; already claimed both the variant heads and the payload types.
   (list eko--pascal-name-regexp '(0 eko-constructor-face))

   ;; Calls and fields ------------------------------------------
   '("\\.\\(\\_<[[:lower:]_]\\(?:\\sw\\|\\s_\\)*\\_>\\)[ \t]*("
     (1 font-lock-function-name-face))
   '("\\.\\(\\_<[[:lower:]_]\\(?:\\sw\\|\\s_\\)*\\_>\\)"
     (1 font-lock-variable-name-face))
   (list (concat "\\(" eko--lower-name-regexp "\\)[ \t]*(")
         '(1 font-lock-function-name-face))

   ;; Wildcard --------------------------------------------------
   '("\\_<_\\_>" 0 font-lock-constant-face)

   ;; Literals --------------------------------------------------
   (list "\\_<0x[[:xdigit:]_]+\\_>" 0 eko--number-face)
   (list "\\_<0b[01_]+\\_>" 0 eko--number-face)
   (list "\\_<[0-9][0-9_]*\\.[0-9][0-9_]*\\(?:[eE][+-]?[0-9_]+\\)?\\_>"
         0 eko--number-face)
   (list "\\_<[0-9][0-9_]*[eE][+-]?[0-9_]+\\_>" 0 eko--number-face)
   (list "\\_<[0-9][0-9_]*\\_>" 0 eko--number-face)

   ;; Operators -------------------------------------------------
   (cons "!->\\|?->\\|->\\|<-" 'eko-arrow-face)
   (cons ":=" 'font-lock-keyword-face)
   (cons "&&\\|||" 'font-lock-keyword-face)
   (cons "\\.\\.=\\|\\.\\." 'font-lock-builtin-face)
   (list "\\(?:)[ \t]*\\)\\([!?]\\)\\(?:[ \t]+=\\|[ \t]*$\\)"
         '(1 eko-arrow-face))

   ;; String interpolation --------------------------------------
   '(eko--match-interpolation (0 'eko-interpolation-face t)))
  "Font lock keywords for `eko-mode'.")

(defvar eko-mode-syntax-table
  (let ((table (make-syntax-table)))
    ;; `//' line comments.  `*' carries no comment syntax, so `/*'
    ;; is not a comment start.
    (modify-syntax-entry ?/ ". 12b" table)
    (modify-syntax-entry ?\n "> b" table)
    (modify-syntax-entry ?\r "> b" table)
    ;; Strings, character literals, and escapes.
    (modify-syntax-entry ?\" "\"" table)
    (modify-syntax-entry ?' "\"" table)
    (modify-syntax-entry ?\\ "\\" table)
    ;; Identifier constituents: `_' joins Unicode word characters;
    ;; `' is a character literal delimiter, not an identifier char.
    (modify-syntax-entry ?_ "_" table)
    ;; All three bracket pairs nest.
    (modify-syntax-entry ?\( "()" table)
    (modify-syntax-entry ?\) ")(" table)
    (modify-syntax-entry ?\[ "(]" table)
    (modify-syntax-entry ?\] ")[" table)
    (modify-syntax-entry ?{ "(}" table)
    (modify-syntax-entry ?} "){" table)
    table)
  "Syntax table used in `eko-mode'.")

(defvar eko-imenu-generic-expression
  `(("Types"
     ,(concat "^[ \t]*\\(" eko--name-regexp
              "\\)[ \t]*:[ \t]*\\(?:ctype\\|type\\)\\_>")
     1)
    ("Tests"
     ,(concat "^[ \t]*\\(" eko--name-regexp
              "\\)[ \t]*:[ \t]*test\\_>")
     1)
    ("Functions"
     ,(concat "^[ \t]*\\(" eko--name-regexp
              "\\)[ \t]*:[ \t]*(")
     1)
    ("Functions"
     ,(concat "^[ \t]*\\(" eko--pascal-name-regexp
              "\\)\\.\\(" eko--lower-name-regexp "\\)[ \t]*:")
     2)
    ;; Annotated value bindings: after the colon the annotation starts
    ;; with a PascalCase type, the unit `_', or a bracketed composite;
    ;; `type' and `test' are lower case and have their own entries.
    ("Bindings"
     ,(concat "^[ \t]*\\(" eko--lower-name-regexp
              "\\)[ \t]*:[ \t]*\\(?:[[:upper:]_]\\|\\[\\)")
     1)
    ("Bindings"
     ,(concat "^[ \t]*\\(" eko--lower-name-regexp
              "\\)[ \t]*:=")
     1))
  "Imenu rules for `eko-mode'.")

;;;; Indentation

(defconst eko--decl-regexp
  "\\`\\(?:\\sw\\|\\s_\\)+[ \t]*:?="
  "Matches a line that binds a name: `NAME: ...' or `NAME := ...'.")

(defconst eko--case-header-regexp
  "\\`case\\>"
  "Matches a line that starts a `case' expression or header.
The scrutinee may continue over several lines before the final `:'.")

(defconst eko--body-intro-regexp
  "\\(->\\|[=:(\\[{|,+]\\)[ \t]*\\'"
  "Matches text ending a line whose body or continuation follows below.
  The introducers are `=' for a definition body, `:' for a callback,
  `if', or `case' block, `->' for an arm body, and an open bracket or
  comma for a continued composite.")

(defun eko--branch-p (code)
  "Non-nil when CODE is a case-arm line: `PATTERN -> BODY'.
The first `->' must sit at bracket depth zero, so the arrow of a
function type inside `(...)' never counts.  A guard (`PATTERN if
COND') is stripped before the `=' check, because a guard is an
ordinary Bool expression and may compare with `=='.  The remaining
pattern must hold no `=' and must not start with `case'."
  (let ((i 0)
        (n (length code))
        (depth 0)
        (arrow nil))
    (while (and (not arrow) (< i n))
      (let ((char (aref code i)))
        (cond
         ((memq char '(?\( ?\[ ?{)) (setq depth (1+ depth)) (setq i (1+ i)))
         ((memq char '(?\) ?\] ?})) (setq depth (1- depth)) (setq i (1+ i)))
         ((and (eq char ?-) (< (1+ i) n) (eq (aref code (1+ i)) ?>))
          (if (zerop depth) (setq arrow i) (setq i (+ i 2))))
         (t (setq i (1+ i))))))
    (when arrow
      (let* ((raw (string-trim (substring code 0 arrow)))
             (pattern (if (string-match "\\`\\(.*?\\)[ \t]+if[ \t]+" raw)
                          (match-string 1 raw)
                        raw)))
        (and (not (string-empty-p pattern))
             (not (string-match-p "=" pattern))
             (not (string-match-p "\\`case\\>" pattern)))))))

(defun eko--code-part (raw)
  "Return RAW with any trailing // comment removed.
Double quotes are respected, so a // inside a string literal stays."
  (let* ((n (length raw))
         (i 0)
         (in-string nil)
         (cut n))
    (while (< i n)
      (let ((ch (aref raw i)))
        (cond
         ((and in-string (eq ch ?\\))
          (setq i (+ i 2)))
         ((eq ch ?\")
          (setq in-string (not in-string))
          (setq i (1+ i)))
         ((and (not in-string)
               (eq ch ?/)
               (< (1+ i) n)
               (eq (aref raw (1+ i)) ?/))
          (setq cut i)
          (setq i n))
         (t
          (setq i (1+ i))))))
    (substring raw 0 cut)))

(defun eko--parse-raw-line (raw)
  "From RAW line text return (INDENT . CODE), or nil for blank/comment lines."
  (let ((code (string-trim (eko--code-part raw))))
    (unless (string-empty-p code)
      (cons (and (string-match "\\`[ \t]*" raw) (match-end 0)) code))))

(defun eko--lines-above (pos)
  "Return the code lines above POS, top to bottom, as (INDENT . CODE).
Blank and comment-only lines are skipped.  Scanning starts at the
nearest line above POS that begins at column 0 (or `point-min')."
  (save-excursion
    (goto-char pos)
    (let (events stop)
      (while (and (not stop) (not (bobp)))
        (forward-line -1)
        (let ((pair (eko--parse-raw-line
                     (buffer-substring-no-properties
                      (line-beginning-position) (line-end-position)))))
          (when pair
            (push pair events)
            (when (zerop (car pair))
              (setq stop t)))))
      events)))

(defun eko--prev-code-line (events)
  "The nearest code line, given EVENTS from `eko--lines-above'."
  (car (last events)))

(defun eko--next-code-line (pos)
  "Return (INDENT . CODE) of the nearest code line below POS, or nil."
  (save-excursion
    (goto-char pos)
    (let (pair)
      (while (and (not pair)
                  (zerop (forward-line 1)))
        (setq pair (eko--parse-raw-line
                    (buffer-substring-no-properties
                     (line-beginning-position) (line-end-position)))))
      pair)))

(defun eko--run-layout (events cur-indent)
  "Fold EVENTS (top to bottom) into a (STACK . CLOSED) pair.
Each stack entry is (KIND . INDENT), with KIND `decl' for `NAME: ...'
lines, `case' for case headers and `body' for a line ending with a
body introducer (`=', `->', lambda `:', a bracket or a comma).
Blocks follow the offside rule: a block closes when a line's original
indent is less than or equal to the block's own indent.  CUR-INDENT is
the current indent of the line being laid out, applied the same way.
CLOSED is the indent of the innermost block CUR-INDENT closed, or nil."
  (let ((stack nil))
    (dolist (ev events)
      (let ((oi (car ev))
            (code (cdr ev)))
        (while (and stack (<= oi (cdar stack)))
          (pop stack))
        (cond
         ((string-match-p eko--case-header-regexp code)
          (push (cons 'case oi) stack))
         ((string-match-p eko--decl-regexp code)
          (push (cons 'decl oi) stack))
         ((string-match-p eko--body-intro-regexp code)
          (push (cons 'body oi) stack)))))
    (let ((closed nil))
      (while (and stack (<= cur-indent (cdar stack)))
        (setq closed (or closed (cdar stack)))
        (pop stack))
      (cons stack closed))))

(defun eko--align-after-equals ()
  "Column of the first text after `=' on the nearest line above with one."
  (save-excursion
    (beginning-of-line)
    (let ((limit (save-excursion
                   (goto-char (max (point-min) (- (point) 500)))
                   (line-beginning-position))))
      (when (re-search-backward "[^<>=!+*/-]=[^=]" limit t)
        (goto-char (1+ (match-beginning 0)))   ; on the `=' itself
        (if (eolp)
            (+ (current-indentation) eko-indent-offset)
          (forward-char 1)
          (skip-chars-forward " \t")
          (if (eolp)
              (+ (current-indentation) eko-indent-offset)
            (current-column)))))))

(defun eko--matching-opener-indent ()
  "Indentation of the line holding the bracket matching a line-initial closer."
  (save-excursion
    (beginning-of-line)
    (skip-chars-forward " \t")
    (when (and (not (eolp))
               (= (char-syntax (char-after)) ?\)))
      (condition-case nil
          (progn
            (backward-up-list 1)
            (current-indentation))
        (error nil)))))

(defun eko--comment-indent (bol)
  "Indentation for the comment-only line at BOL.
Comments are free-form: the author's column is kept, and only the
code below can push the comment further right."
  (save-excursion
    (goto-char bol)
    (let ((next (eko--next-code-line bol)))
      (max (current-indentation)
           (if next (car next) (current-indentation))))))

(defun eko--blank-indent (bol)
  "Indentation for a blank line at BOL, following the code around it.
A line typed after a block introducer (`=', `->', a lambda/`if'/
`case' `:') lands one step deeper, so `newline-and-indent' starts in
the new block.
`indent-region' skips blank lines, so a region never gains whitespace."
  (let* ((events (eko--lines-above bol))
         (prev (eko--prev-code-line events))
         (prev-indent (if prev (car prev) 0))
         (next (eko--next-code-line bol)))
    (cond
     ((null next)
      (if (and prev (string-match-p eko--body-intro-regexp (cdr prev)))
          (+ prev-indent eko-indent-offset)
        prev-indent))
     ((null prev) (car next))
     (t (max prev-indent (car next))))))

(defun eko--code-indent (bol code)
  "Column to indent the code line CODE starting at BOL, or nil for none."
  (let* ((cur-indent (current-indentation))
         (events (eko--lines-above bol))
         (prev (eko--prev-code-line events))
         (prev-indent (if prev (car prev) 0))
         (prev-code (if prev (cdr prev) ""))
         (layout (eko--run-layout events cur-indent))
         (stack (car layout))
         (closed (cdr layout))
         (top (car stack)))
    (cond
     ;; Continuation of a `|' union type: align after `='.
     ((string-prefix-p "|" code)
      (or (eko--align-after-equals)
          (+ prev-indent eko-indent-offset)))
     ;; Line starting with a closing bracket: outdent to its opener.
     ((string-match-p "\\`[]})]" code)
      (or (eko--matching-opener-indent) prev-indent))
     ;; Case branch: one step under the innermost open case header.
     ((and (eko--branch-p code) top (eq (car top) 'case))
      (+ (cdr top) eko-indent-offset))
     ;; The line closes blocks: it starts the next item in the block
     ;; that survived the dedent -- an arm under a case, a statement
     ;; under a body.  (The innermost closed block's own indent is
     ;; wrong here: an arm-body block between the case and this line
     ;; would pull the line into the arms.)
     ((and closed stack)
      (+ (cdr (car stack)) eko-indent-offset))
     ;; Top level declarations and case expressions start at column 0.
     ((and (null stack)
           (or (string-match-p eko--decl-regexp code)
               (string-match-p eko--case-header-regexp code)))
      0)
     ;; The previous line opens a block body (ends with `=', `->',
     ;; `:', a bracket or a comma): indent one step.
     ((string-match-p eko--body-intro-regexp prev-code)
      (+ prev-indent eko-indent-offset))
     ;; Otherwise: a top level statement, or a plain continuation
     ;; of the line above inside the block that is still open.
     ((null stack) 0)
     (t prev-indent))))

(defun eko--calculate-indent ()
  "Return the column to indent the current line to, or nil to leave it alone."
  (save-excursion
    (beginning-of-line)
    (let* ((bol (point))
           (raw (buffer-substring-no-properties bol (line-end-position)))
           (code (string-trim (eko--code-part raw))))
      (cond
       ;; Inside a multiline string: leave the line alone.
       ((nth 3 (syntax-ppss bol))
        nil)
       ;; Comment-only line: deepened to the code below, never outdented.
       ((string-match-p "\\`[ \t]*//" raw)
        (eko--comment-indent bol))
       ;; Blank line: follow the code around it.
       ((string-empty-p code)
        (eko--blank-indent bol))
       (t
        (eko--code-indent bol code))))))

(defun eko-indent-line ()
  "Indent the current line as Eko code."
  (interactive)
  (let ((indent (eko--calculate-indent))
        (rel (- (current-column) (current-indentation))))
    (when indent
      (indent-line-to indent)
      (when (> rel 0)
        (move-to-column (+ indent rel) t)))))

;;;; Entry point

;;;###autoload
(define-derived-mode eko-mode prog-mode "Eko"
  "Major mode for editing Eko source code (\\.eko files).

Eko is a statically typed, immutable language.  Bindings are
`name = value', `name := value' (rebinding), and `name <- call'
(inverted binding); declarations put `type', `ctype', or `test' in
the annotation slot; `if' is sugar over `case', whose arms are
`PATTERN -> body'; composites use `[...]' at type position and
`{...}' at value position; arrows carry the world bit (`!->', `?->',
or the elided `!', `?').

Highlighting is role-aware: inside type spans PascalCase fonts as a
type and lowercase as an italic type variable, while variant heads
and PascalCase in bodies font as constructors (teal, the family
`True'/`False' live in).  The palette is buffer-local and adapts to
light themes; see the `eko' customize group.

Special commands:
\\{eko-mode-map}

Indentation is done with `eko-indent-line' (normally bound to TAB);
the indentation step is `eko-indent-offset' (4 by default)."
  :syntax-table eko-mode-syntax-table
  :group 'eko
  (setq-local comment-start "// ")
  (setq-local comment-end "")
  (setq-local comment-start-skip "//+[ \t]*")
  (setq-local indent-tabs-mode nil)
  (setq-local indent-line-function #'eko-indent-line)
  (setq-local font-lock-defaults '(eko-mode-font-lock-keywords nil nil))
  (setq-local imenu-generic-expression eko-imenu-generic-expression)
  ;; Type spans are found from the declaration that opens them, so each
  ;; fontify pass is widened to whole top-level declarations first.
  (add-hook 'font-lock-extend-region-functions #'eko--extend-region nil t)
  ;; Pin the reference palette without touching other modes: the
  ;; remappings live only in Eko buffers.
  (eko--apply-palette))

;;;###autoload
(add-to-list 'auto-mode-alist '("\\.eko\\'" . eko-mode))

(provide 'eko-mode)

;;; eko-mode.el ends here
