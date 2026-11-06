# rt — ekort

The runtime library: reference counting, heap value families, scheduler,
veneer. Permanent C (ARCHITECTURE.md).

## Veneer

The embedding veneer: the entry points a host links against, and
nothing else (Clause 10.7.1). The value and module types are
opaque; layouts belong to the implementation. A value that crosses
the boundary arrives at no reference of its own: the host holds it
with eko_retain and drops the hold with eko_release (Clause
10.7.2).
