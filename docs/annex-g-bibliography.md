# Annex G. Bibliography

This annex is informative. It lists works that inform the design of Eko and
the prior-art languages discussed in the design record or used at the native
boundary. Citations identify the work and, where applicable, the edition
consulted; they are bibliographic pointers, not requirements. No URL is
included. The normative references of this document are listed in Clause 3 and
are not repeated here; Clause 3 governs their application. The design record
is not a published work and is indexed in Annex E.

## G.1 Type Systems

These works inform nominal type identity (Clause 7.5), parametric generics and
implicit generalization (Clause 7.6), local inference at annotated bindings,
exhaustiveness checking (Clause 7.7), and the decision to bound subtyping to
bit adaptation and the bottom type.

- Cardelli, Luca, and Peter Wegner. "On Understanding Types, Data Abstraction,
  and Polymorphism." *ACM Computing Surveys* 17(4), 1985.
- Damas, Luis, and Robin Milner. "Principal Type-Schemes for Functional
  Programs." *Proceedings of the 9th ACM SIGPLAN-SIGACT Symposium on
  Principles of Programming Languages*, 1982.
- Hindley, J. Roger. "The Principal Type-Scheme of an Object in Combinatory
  Logic." *Transactions of the American Mathematical Society* 146, 1969.
- Liskov, Barbara, and Jeannette Wing. "A Behavioral Notion of Subtyping."
  *ACM Transactions on Programming Languages and Systems* 16(6), 1994.
- Maranget, Luc. "Warnings for Pattern Matching." *Journal of Functional
  Programming* 17(3), 2007.
- Milner, Robin. "A Theory of Type Polymorphism in Programming." *Journal of
  Computer and System Sciences* 17(3), 1978.
- Pierce, Benjamin C. *Types and Programming Languages*. MIT Press, 2002.
- Pierce, Benjamin C., and David N. Turner. "Local Type Inference." *ACM
  Transactions on Programming Languages and Systems* 22(1), 2000.
- Reynolds, John C. "Towards a Theory of Type Structure." *Programming
  Symposium*, Lecture Notes in Computer Science 19, Springer, 1974.
- Wadler, Philip, and Stephen Blott. "How to Make Ad-Hoc Polymorphism Less Ad
  Hoc." *Proceedings of the 16th ACM SIGPLAN-SIGACT Symposium on Principles of
  Programming Languages*, 1989.
- Wright, Andrew K., and Matthias Felleisen. "A Syntactic Approach to Type
  Soundness." *Information and Computation* 115(1), 1994.

## G.2 Effects and Capabilities

These works inform the separation of authority from the world bit (Clause
7.9), the treatment of callbacks as bit parameters, and the rejection of label
rows and handlers.

- Dennis, Jack B., and Earl C. Van Horn. "Programming Semantics for
  Multiprogrammed Computations." *Communications of the ACM* 9(3), 1966.
- Gifford, David K., and John M. Lucassen. "Integrating Functional and
  Imperative Programming." *Proceedings of the 1986 ACM Conference on LISP and
  Functional Programming*, 1986.
- Leijen, Daan. "Koka: Programming with Row Polymorphic Effect Types."
  *Proceedings of the 5th Workshop on Mathematically Structured Functional
  Programming*, Electronic Proceedings in Theoretical Computer Science 153,
  2014.
- Levy, Henry M. *Capability-Based Computer Systems*. Digital Press, 1984.
- Lindley, Sam, Conor McBride, and Craig McLaughlin. "Do Be Do Be Do."
  *Proceedings of the 44th ACM SIGPLAN Symposium on Principles of Programming
  Languages*, 2017.
- Lucassen, John M., and David K. Gifford. "Polymorphic Effect Systems."
  *Proceedings of the 15th ACM SIGPLAN-SIGACT Symposium on Principles of
  Programming Languages*, 1988.
- Miller, Mark S. *Robust Composition: Towards a Unified Approach to Access
  Control and Concurrency Control*. PhD dissertation, Johns Hopkins
  University, 2006.
- Plotkin, Gordon, and Matija Pretnar. "Handlers of Algebraic Effects."
  *Programming Languages and Systems*, Lecture Notes in Computer Science 5502,
  Springer, 2009.
- Saltzer, Jerome H., and Michael D. Schroeder. "The Protection of Information
  in Computer Systems." *Proceedings of the IEEE* 63(9), 1975.
- Talpin, Jean-Pierre, and Pierre Jouvelot. "The Type and Effect Discipline."
  *Information and Computation* 111(2), 1994.

## G.3 Memory Management

These works inform reference counting, the acyclicity argument (Clause 8.6),
and the exclusion of tracing collection, regions, and ownership annotations
from the language.

- Bacon, David F., and V. T. Rajan. "Concurrent Cycle Collection in Reference
  Counted Systems." *ECOOP 2001 — Object-Oriented Programming*, Lecture Notes
  in Computer Science 2072, Springer, 2001.
- Bacon, David F., Perry Cheng, and V. T. Rajan. "A Unified Theory of Garbage
  Collection." *Proceedings of the 19th Annual ACM Conference on
  Object-Oriented Programming, Systems, Languages, and Applications*, 2004.
- Boyapati, Chandrasekhar, Robert Lee, and Martin Rinard. "Ownership Types for
  Safe Programming: Preventing Data Races and Deadlocks." *Proceedings of the
  17th ACM SIGPLAN Conference on Object-Oriented Programming, Systems,
  Languages, and Applications*, 2002.
- Collins, George E. "A Method for Overlapping and Erasure of Lists."
  *Communications of the ACM* 3(12), 1960.
- Grossman, Dan, Greg Morrisett, Trevor Jim, Michael Hicks, Yanling Wang, and
  James Cheney. "Region-Based Memory Management in Cyclone." *Proceedings of
  the ACM SIGPLAN 2002 Conference on Programming Language Design and
  Implementation*, 2002.
- Jones, Richard, Antony Hosking, and Eliot Moss. *The Garbage Collection
  Handbook: The Art of Automatic Memory Management*. CRC Press, 2011.
- Jung, Ralf, Jacques-Henri Jourdan, Robbert Krebbers, and Derek Dreyer.
  "RustBelt: Securing the Foundations of the Rust Programming Language."
  *Proceedings of the ACM on Programming Languages* 2(POPL), 2018.
- Tofte, Mads, and Jean-Pierre Talpin. "Region-Based Memory Management."
  *Information and Computation* 132(2), 1997.

## G.4 Native Interface

These works inform extern declarations, the marshalling whitelist, the audited
`Pure` and `C.In` guarantees, and the trusted base of Clause 10.1.4.

- Blume, Matthias. "No-Longer-Foreign: Teaching an ML Compiler to Speak C
  'Natively'." *Electronic Notes in Theoretical Computer Science* 59(1), 2001.
- Boehm, Hans-J. "Threads Cannot Be Implemented as a Library." *Proceedings of
  the ACM SIGPLAN 2005 Conference on Programming Language Design and
  Implementation*, 2005.
- Furr, Michael, and Jeffrey S. Foster. "Checking Type Safety of Foreign
  Function Calls." *Proceedings of the ACM SIGPLAN 2005 Conference on
  Programming Language Design and Implementation*, 2005.
- Necula, George C. "Proof-Carrying Code." *Proceedings of the 24th ACM
  SIGPLAN-SIGACT Symposium on Principles of Programming Languages*, 1997.

## G.5 Module Systems and Build Graphs

These works inform file-is-module (Clause 7.2), interface extraction, separate
compilation, and the acyclic build graph of Clause 11.6.

- Cardelli, Luca. "Program Fragments, Linking, and Modularization."
  *Proceedings of the 24th ACM SIGPLAN-SIGACT Symposium on Principles of
  Programming Languages*, 1997.
- Cardelli, Luca, and Xavier Leroy. "Abstract Types and the Dot Notation."
  *Proceedings of the IFIP TC2 Working Conference on Programming Concepts and
  Methods*, 1990.
- Dolstra, Eelco. *The Purely Functional Software Deployment Model*. PhD
  dissertation, Utrecht University, 2006.
- Harper, Robert, and Mark Lillibridge. "A Type-Theoretic Approach to
  Higher-Order Modules with Sharing." *Proceedings of the 21st ACM
  SIGPLAN-SIGACT Symposium on Principles of Programming Languages*, 1994.
- Leroy, Xavier. "Manifest Types, Modules, and Separate Compilation."
  *Proceedings of the 21st ACM SIGPLAN-SIGACT Symposium on Principles of
  Programming Languages*, 1994.
- Leroy, Xavier. "A Modular Module System." *Journal of Functional
  Programming* 10(3), 2000.
- MacQueen, David B. "Modules for Standard ML." *Proceedings of the 1984 ACM
  Symposium on LISP and Functional Programming*, 1984.
- Mokhov, Andrey, Neil Mitchell, and Simon Peyton Jones. "Build Systems à la
  Carte." *Proceedings of the ACM on Programming Languages* 2(ICFP), 2018.

## G.6 Prior-Art Languages

The following languages are discussed in the design record or used at the
native boundary. The citations identify the language definition or standard
reference used for comparison; they do not imply endorsement of the language's
rules.

- Apple Inc. *The Swift Programming Language*. Apple Books, 2014.
- Bezanson, Jeff, Alan Edelman, Stefan Karpinski, and Viral B. Shah. "Julia: A
  Fresh Approach to Numerical Computing." *SIAM Review* 59(1), 2017.
- Bierman, Gavin, Martín Abadi, and Mads Torgersen. "Understanding
  TypeScript." *ECOOP 2014 — Object-Oriented Programming*, Lecture Notes in
  Computer Science 8586, Springer, 2014.
- Czaplicki, Evan. "Elm: Concurrent FRP for Functional GUIs." Senior thesis,
  Harvard University, 2012.
- Donovan, Alan A. A., and Brian W. Kernighan. *The Go Programming Language*.
  Addison-Wesley, 2015.
- Ecma International. *ECMAScript Language Specification*. Standard ECMA-262,
  15th edition, 2024.
- Gosling, James, Bill Joy, Guy Steele, Gilad Bracha, and Alex Buckley. *The
  Java Language Specification, Java SE 8 Edition*. Addison-Wesley, 2014.
- Hejlsberg, Anders, Mads Torgersen, Scott Wiltamuth, and Peter Golde. *The C#
  Programming Language*. Addison-Wesley, 2003.
- Jemerov, Dmitry, and Svetlana Isakova. *Kotlin in Action*. Manning, 2017.
- Kernighan, Brian W., and Dennis M. Ritchie. *The C Programming Language*.
  Prentice Hall, 1978; second edition, 1988.
- Klabnik, Steve, and Carol Nichols. *The Rust Programming Language*. No
  Starch Press, 2018.
- Matsumoto, Yukihiro. *Ruby in a Nutshell*. O'Reilly Media, 2001.
- Odersky, Martin, Lex Spoon, and Bill Venners. *Programming in Scala*.
  Artima, 2008.
- Paulson, Lawrence C. *ML for the Working Programmer*. Cambridge University
  Press, second edition, 1996.
- Peyton Jones, Simon, ed. *Haskell 98 Language and Libraries: The Revised
  Report*. Cambridge University Press, 2003.
- Pilfold, Louis. *Gleam Language Reference*. Gleam Project, 2024.
- Van Rossum, Guido, and Fred L. Drake Jr. *Python Reference Manual*. Centrum
  voor Wiskunde en Informatica, 1995.
- Zig Software Foundation. *Zig Language Reference*. Zig Software Foundation,
  2023.
