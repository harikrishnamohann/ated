# Clementine: A Collaborative, TUI-Oriented Modal Text Editor

**Team Members**
- **Ashima** — PRP23CS019
- **Hajira** — PRP23CS040
- **Hari** — PRP23CS042

---
# Proposal
## Demo
**Abey:** Dude, can I help you set up your server config?
**Hari:** Sure. Just do `ssh hk@nixos` and join me with `clem --attach config`.
**Abey's view:**
```text
$ ssh hk@nixos
$ clem --attach config

┌─ config.nix ─────────────────────────────────────────────┐
│ 1  services.nginx.enable = true;                         │
│ 2  services.nginx.virtualHosts."example.com" = {         │
│ 3    root = "/var/www/example";█ ← Abey                 │
│ 4  };                                                    │
└──────────────────────────────────────────────────────────┘
```

Abey connects to Hari's machine over SSH and joins the existing Clementine session. Both users edit the same document in real time, with each user having an independently rendered cursor.

No file transfer, screen sharing, browser, account, or third-party collaboration service is required.

---
## Summary

**Clementine** is a terminal-native collaborative text editor that combines a modal editing interface with a rope-based text representation, a CRDT-based collaboration layer, and local IPC.

The project is primarily an exploration of systems and data-structure design rather than an attempt to replace established editors or collaboration platforms.

The central engineering challenge is to make several independent components work together as a coherent system:

- a rope for efficient text storage and modification,
    
- a CRDT for representing and merging concurrent edits,
    
- a modal editing state machine,
    
- a terminal user interface capable of displaying multiple users,
    
- and a session process that coordinates clients through IPC.
    

The project will be implemented in **C** using POSIX APIs and a terminal UI library such as `ncurses`.

---

# Motivation and Positioning

Clementine does not claim to solve an urgent or widely reported problem. Existing tools such as `tmux`, VS Code Live Share, and browser-based collaborative editors already address many real-world collaboration requirements.

Instead, the value of Clementine is in the engineering problem itself.

The project provides an opportunity to study and implement, within one system:

- efficient text data structures,
    
- concurrent replicated state,
    
- terminal input and rendering,
    
- event-driven network/IPC programming,
    
- modal editor design,
    
- synchronization protocols,
    
- and fault handling.
    

The final system therefore serves as both a usable terminal editor and a practical demonstration of how these systems concepts interact.

---

# Use Cases

### Pair programming on remote servers

Two developers can edit the same file on a remote machine without sharing an entire terminal session.

### Teaching and mentoring

A mentor and student can work on the same source file while retaining separate cursors and input states.

### Remote interview environments

An interviewer and candidate can work together on a source file through an existing SSH connection.

### Restricted infrastructure

Clementine can be used in environments where browser-based IDEs or third-party collaboration services cannot be installed, provided users already have SSH access.

### Collaborative system administration

Multiple administrators can inspect and modify configuration files together during setup, maintenance, or incident response.

### Research environments

Researchers working on shared servers can collaboratively modify scripts and configuration files without requiring an additional external collaboration service.

---

# Project Goals

The primary goal is to build a functional prototype that demonstrates the integration of the following components:

1. **Rope-based text storage**
    
2. **CRDT-based collaborative editing**
    
3. **Modal terminal editing**
    
4. **Multi-user terminal rendering**
    
5. **Local IPC and session management**
    
6. **Event-driven client handling**
    
7. **File persistence**
    
8. **Testing and performance evaluation**
    

The project will prioritize correctness, maintainability, and clear separation between components over feature breadth.

---

# Architecture

Clementine separates document state, client state, and session management.

```text
                         SSH
                  ┌───────────────┐
                  │               │
               User A          User B
                  │               │
                  ▼               ▼
            Clementine       Clementine
              Client            Client
                  │               │
                  └───────┬───────┘
                          │
                   Local IPC / Socket
                          │
                   ┌──────▼──────┐
                   │   Session   │
                   │   Process   │
                   │             │
                   │    CRDT     │
                   │      │      │
                   │     Rope    │
                   │      │      │
                   │   Document  │
                   └─────────────┘
```

SSH is used only as the remote access mechanism. Clementine itself does not implement an SSH server.

Once users are on the same host, their Clementine client processes communicate with a session process through a local IPC mechanism such as a Unix domain socket.

The session process maintains the shared document state and coordinates operations between connected clients.

---

# System Components

## 1. Text Representation — Rope

The document will be represented using a **rope**, a tree-based text data structure.

Instead of storing the entire file in one contiguous memory region, the text is divided into smaller chunks stored in leaf nodes. Internal nodes maintain metadata describing the text contained in their subtrees.

A simplified representation is:

```text
                 Root
                /    \
               /      \
           Node        Node
          /   \       /    \
       "hello" " "  "world" "\n"
```

Operations such as insertion and deletion can modify only the relevant parts of the tree rather than repeatedly shifting the entire document.

The rope implementation will be developed as part of the project rather than relying on an existing rope library.

The implementation will be evaluated using benchmarks comparing common editing operations against a simpler contiguous-buffer representation.

---

## 2. Collaborative Document Model — CRDT

The CRDT provides the semantics required for concurrent editing.

An edit will be represented as an operation rather than simply modifying a local character array.

Conceptually:

```text
Insert(...)
Delete(...)
```

Each operation will contain enough information to identify its origin and position in the replicated document state.

When two clients perform edits concurrently, the CRDT determines how those operations are ordered and merged so that replicas eventually converge to the same document state.

The CRDT will be responsible for:

- identifying operations,
    
- maintaining causal relationships where required,
    
- ordering concurrent edits,
    
- applying remote operations,
    
- handling duplicate or out-of-order operations,
    
- and guaranteeing convergence under the supported failure model.
    

The exact CRDT design will be selected after studying sequence CRDT approaches such as RGA and Fugue.

---

## 3. CRDT and Rope Relationship

The CRDT and rope will have separate responsibilities.

The **CRDT represents the logical collaborative document state**, while the **rope provides an efficient local representation of the resulting text**.

Conceptually:

```text
             CRDT
              │
       collaborative state
              │
              ▼
             Rope
              │
        current document
              │
              ▼
           Renderer
```

This separation avoids making the rope responsible for distributed consistency and avoids making the CRDT responsible for efficient terminal rendering.

The exact interface between the two components will be designed so that either component can be tested independently.

---

## 4. Editing Engine

The editing engine translates user actions into document operations.

For example:

```text
keypress
   ↓
modal input system
   ↓
editing command
   ↓
insert/delete/move
   ↓
document operation
```

The editing engine will not depend on whether an operation originated from local keyboard input or a remote client.

This separation allows local and remote edits to pass through the same document manipulation mechanisms.

---

## 5. Modal Input System

Clementine will use a modal editing model inspired by editors such as Vim and Helix.

A state machine will maintain the current mode and interpret input accordingly.

For example:

```text
             ┌──────────────┐
             │ Navigation   │
             └──────┬───────┘
                    │ i
                    ▼
             ┌──────────────┐
             │    Insert    │
             └──────┬───────┘
                    │ Esc
                    ▼
             ┌──────────────┐
             │ Navigation   │
             └──────────────┘
```

The initial implementation will focus on a small, well-defined set of navigation, insertion, and deletion commands rather than attempting to reproduce the complete feature set of an existing modal editor.

---

## 6. Client State

Document state and user-interface state will be kept separate.

The shared document contains information such as:

```text
CRDT
Rope
File contents
```

Each connected client maintains its own state:

```text
Cursor position
Selection
Editing mode
Viewport
Terminal dimensions
```

This allows multiple users to edit the same document while retaining independent cursors and modes.

---

## 7. Renderer

The renderer converts the current document and client state into terminal output.

It will be responsible for:

- displaying visible lines,
    
- displaying line numbers,
    
- positioning the local cursor,
    
- displaying remote cursors,
    
- handling terminal dimensions,
    
- and updating only when the relevant state changes.
    

Syntax highlighting is considered a **stretch goal** rather than a core requirement.

---

## 8. Session Management and IPC

A Clementine session will manage the shared document and connected clients.

For example:

```text
clem config.nix
```

can create or open a session, while:

```text
clem --attach config
```

can connect to an existing session.

The session process will communicate with clients through a local IPC mechanism, preferably a Unix domain socket.

The IPC protocol will carry messages such as:

```text
JOIN
LEAVE
INSERT
DELETE
CURSOR_UPDATE
SYNC
ERROR
```

The protocol will be designed explicitly rather than passing arbitrary serialized internal structures between processes.

---

## 9. Main Event Loop

Clementine will use a single-threaded event loop.

Conceptually:

```text
                 ┌──────────────┐
                 │   poll()     │
                 └──────┬───────┘
                        │
             ┌──────────┼──────────┐
             ▼          ▼          ▼
         terminal    client A   client B
           input       IPC        IPC
             │          │          │
             └──────────┼──────────┘
                        ▼
                    update state
                        │
                        ▼
                     render
                        │
                        └──────► poll()
```

A single event loop avoids introducing unnecessary thread synchronization around shared CRDT and rope state.

`poll()` will initially be preferred over more complex mechanisms such as `epoll`, since the expected number of simultaneous collaborators is small.

---

# Text and Unicode

Clementine will store text as UTF-8.

The initial goal is to preserve valid UTF-8 data during editing rather than implementing every aspect of Unicode text layout.

In particular, full grapheme-cluster handling, complex text shaping, and complete Unicode display semantics are outside the initial scope.

The project will document the exact unit used for cursor positions and editing operations.

---

# Collaboration Model

A basic collaboration scenario is:

```text
Initial document:

abc

Client A: insert X
Client B: insert Y
```

The clients may receive the operations in different orders.

The CRDT must ensure that, after all operations have been delivered:

```text
Document(A) == Document(B)
```

The system will test this property using automatically generated and manually constructed concurrent-edit scenarios.

The initial system will assume that communication eventually delivers valid operations, while still handling disconnects, duplicate messages, and malformed messages gracefully.

---

# Persistence

The initial implementation will maintain the active CRDT and rope state in memory and periodically or explicitly write the resulting document to the original file.

Persistent CRDT operation history and crash recovery are outside the initial scope.

This keeps the first implementation focused on collaborative editing rather than distributed durable storage.

---

# Error Handling

The system will explicitly handle common failure cases, including:

- invalid command-line arguments,
    
- missing files,
    
- permission errors,
    
- unavailable sessions,
    
- duplicate session names,
    
- IPC/socket failures,
    
- client disconnections,
    
- malformed messages,
    
- invalid editing operations,
    
- terminal resizing,
    
- and terminal restoration after abnormal termination.
    

Particular care will be taken to restore the terminal to a usable state when Clementine exits unexpectedly.

---

# Testing

Testing will be divided according to the major system components.

## Rope tests

- insertion
    
- deletion
    
- random access
    
- splitting and joining nodes
    
- boundary conditions
    
- empty documents
    
- large documents
    
- structural invariants
    

## CRDT tests

- sequential edits
    
- concurrent insertions
    
- concurrent deletions
    
- out-of-order operations
    
- duplicate operations
    
- operation replay
    
- convergence
    

A key invariant will be:

```text
After all operations are delivered:

state(client A) == state(client B)
```

## Editor tests

- mode transitions
    
- cursor movement
    
- insertion
    
- deletion
    
- selections
    
- command handling
    

## IPC tests

- session creation
    
- client joining
    
- client leaving
    
- multiple simultaneous clients
    
- malformed messages
    
- connection failure
    

## Integration tests

The final system will be tested with multiple Clementine clients connected to the same session and editing the same document concurrently.

---

# Performance Evaluation

The rope implementation will be evaluated against a simpler contiguous text buffer.

Tests will measure operations such as:

- insertion near the beginning,
    
- insertion near the middle,
    
- insertion near the end,
    
- deletion from the middle,
    
- random access,
    
- and repeated edits.
    

Tests will use documents of different sizes, for example:

```text
1 MB
10 MB
100 MB
```

The goal is not to claim that Clementine is universally faster than existing editors, but to demonstrate the practical effect of the chosen text representation.

The collaborative layer will additionally be evaluated for:

- operation processing time,
    
- synchronization latency,
    
- memory usage,
    
- and behavior as the number of connected clients increases.
    

---

# Scope

## Minimum Viable Product

The following features are considered essential:

- terminal user interface,
    
- file loading and saving,
    
- modal editing,
    
- rope-based text storage,
    
- CRDT-based collaborative operations,
    
- session management,
    
- Unix-domain IPC,
    
- at least two simultaneous clients,
    
- remote cursor rendering,
    
- SSH-based access to a host session,
    
- basic UTF-8 preservation,
    
- convergence tests,
    
- rope benchmarks,
    
- and basic error handling.
    

## Stretch Goals

Depending on available time, the following may be implemented:

- syntax highlighting,
    
- collaborative selections,
    
- persistent sessions,
    
- reconnection,
    
- local undo/redo,
    
- advanced Unicode grapheme handling,
    
- multiple buffers/files,
    
- crash recovery,
    
- and more advanced editor commands.
    

## Non-Goals

Clementine will not initially attempt to provide:

- a graphical interface,
    
- a built-in SSH server,
    
- user accounts or authentication,
    
- cloud synchronization,
    
- a third-party collaboration server,
    
- voice/video communication,
    
- a complete IDE,
    
- or fully featured collaborative undo/redo.
    

SSH authentication is delegated to the operating system's existing SSH infrastructure.

---

# Development Plan

## Phase 1 — Terminal Foundation

- terminal initialization and raw mode
    
- keyboard input
    
- screen rendering
    
- file loading/saving
    
- basic cursor movement
    

## Phase 2 — Rope

- rope node representation
    
- insertion
    
- deletion
    
- traversal
    
- balancing/rebalancing strategy
    
- testing
    
- performance benchmarks
    

## Phase 3 — Modal Editor

- editor state machine
    
- navigation mode
    
- insert mode
    
- basic commands
    
- selections
    
- editing engine
    

## Phase 4 — Session and IPC

- session creation
    
- client attachment
    
- Unix-domain socket communication
    
- message protocol
    
- client management
    
- event loop
    

## Phase 5 — CRDT

- operation representation
    
- identifiers
    
- ordering
    
- concurrent operations
    
- merging
    
- convergence tests
    

## Phase 6 — Collaborative UI

- remote cursor state
    
- remote cursor rendering
    
- client synchronization
    
- terminal resizing
    
- disconnect handling
    

## Phase 7 — Integration and Evaluation

- complete SSH workflow
    
- file persistence
    
- error handling
    
- performance testing
    
- stress testing
    
- documentation
    
- final demonstration
    

---

# Team Responsibilities

Each member will contribute to the overall system, while taking primary responsibility for specific components.

|Member|Primary responsibility|
|---|---|
|**Ashima**|Rope and text representation|
|**Hajira**|CRDT and synchronization|
|**Hari**|TUI, modal editor, and rendering|
|**All members**|IPC, integration, testing, debugging, and documentation|

The responsibilities are intended as primary ownership rather than isolated modules; all members will participate in integration and code review.

---

# Literature and Related Work

### Diamond Types

[Diamond Types](https://github.com/josephg/diamond-types) is a practical collaborative text-editing system that combines CRDT techniques with efficient text representation. It provides useful implementation ideas for the relationship between collaborative operations and text storage.

### CRDTs Go Brrr

Joseph Gentle's article, _CRDTs Go Brrr_, provides practical discussion of CRDT implementation and performance considerations. It will be used as supplementary material when evaluating possible sequence-CRDT designs.

### Replicated Abstract Data Types

This work provides the theoretical foundation for understanding replicated data structures and eventual convergence.

### Fugue

Fugue provides a modern approach to sequence CRDT design and will be studied when selecting the ordering and identification strategy for Clementine's collaborative text operations.

### Existing Terminal Editors

Editors such as Vim and Helix will be studied primarily for their modal interaction models and terminal UI behavior rather than as implementation dependencies.

---

# Implementation

## Chosen Language

C has been selected as the implementation language because Clementine is fundamentally a systems-oriented project.

Using C provides direct exposure to:

- memory management,
    
- data structures,
    
- pointers and ownership,
    
- POSIX APIs,
    
- sockets and IPC,
    
- terminal control,
    
- event loops,
    
- serialization,
    
- and error handling.
    

The project will prioritize maintainability despite using a low-level language.

Code will be organized into small modules with explicit interfaces and ownership rules. Components such as the rope and CRDT will have independent test suites so that correctness can be established before full system integration.

---

# Expected Outcome

The final result will be a working prototype of Clementine capable of the following workflow:

```text
Host:

$ clem config.nix
```

Another user accesses the same machine:

```text
$ ssh host
$ clem --attach config
```

Both clients then interact with a shared session:

```text
             ┌───────────────┐
             │ Shared Session │
             │               │
             │     CRDT      │
             │       │       │
             │      Rope     │
             └───────┬───────┘
                     │
             ┌───────┴───────┐
             │               │
          Client A        Client B
          Cursor A        Cursor B
             │               │
           TUI A             TUI B
```

Concurrent edits are represented as CRDT operations, the resulting document is maintained using the rope, and each client independently renders its own view of the shared document and the other users' cursors.

The project will ultimately demonstrate how efficient text structures, replicated data structures, modal interaction, terminal rendering, and Unix systems programming can be combined into a complete collaborative application.