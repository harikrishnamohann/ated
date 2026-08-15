---
Title: clementine
Description: A Collaborative, TUI-Oriented Modal Text Editor.
Team members: Ashima - PRP23CS019 | Hajira - PRP23CS040 | Hari - PRP23CS042
---
---
# \[Proposal]
### Demo
**Abey:** Dude, can I help you set up your server config?
**Hari:** Sure, just do `ssh hk@nixos` and join me on it with `editor --attach config`.

**Abey's view:**
```
$ ssh hk@nixos
$ clem --attach config

┌─ config.nix ─────────────────────────────────────────────┐
│ 1  services.nginx.enable = true;                         │
│ 2  services.nginx.virtualHosts."example.com" = {         │
│ 3    root = "/var/www/example";█ ← (Abey)                │
│ 4  };                                                    │
└──────────────────────────────────────────────────────────┘
```

Abey connects to Hari's computer over SSH and joins the running session directly. Both users see the same file update in real time, each with their own visible cursor. No file transfer, no screen sharing, and no third-party service is involved.
### Summary
We are building a terminal-native collaborative text editor that demonstrates how efficient text structures (ropes), CRDTs, modal editing, and local IPC can be integrated into a single coherent system. Our goal is not to replace existing editors, but to explore the design of collaborative editing in environments where only a terminal is available.
### Use Cases
- **Pair programming on remote servers** — two developers editing the same file live over SSH, without exposing an entire shared terminal session.
- **Teaching and mentoring** — an instructor or mentor editing alongside a student on the same remote machine, in real time.
- **Remote interview environments** — a candidate and interviewer working in the same file during a technical interview conducted over SSH.
- **Working on restricted infrastructure** — environments where installing a browser-based IDE or third-party collaboration tool is not permitted, but SSH access is already available.
- **Collaborative sysadmin work** — multiple administrators editing a configuration file together during setup or an incident, instead of handing control back and forth.
- **Research environments** — collaborators on shared research servers working on the same script or configuration without additional tooling.

### Scope and Positioning
This project does not address an urgent or widely reported problem. Existing tools such as `tmux attach`, VS Code Live Share, and various browser-based platforms already provide adequate collaborative editing for most users. Accordingly, this project is not positioned as solving an unmet market need. Its value lies in the engineering work itself: designing a rope from scratch, integrating a CRDT into it, building a modal editing engine, rendering multiple cursors in a terminal, and connecting all of this through local IPC, as a complete system built within one semester.

### What We Are Building
A terminal-based text editor, similar in spirit to editors such as Vim or Helix, with real-time multi-user editing built into its core design using SSH and a session handling mechanism. No account, browser, or external server is required. Any user with SSH access to the machine can join a session.

### Architecture of the Editor
The editor will be implemented in C, giving a straightforward fit with POSIX APIs for Unix domain sockets and terminal rendering (via `ncurses` or a similar library). Peer connections will be handled through a single-threaded event loop (using `select`/`poll`/`epoll`) rather than multithreading, to avoid the added complexity of synchronizing shared rope/CRDT state across threads.

**1. Text buffer (the rope)** The file's contents are stored as a tree, where each leaf node holds a small chunk of text and each parent node tracks the total size of the text beneath it. Locating or editing a specific position requires traversing only the relevant path of the tree, rather than scanning the entire file. This keeps editing responsive even on large files.

**2. Editing engine** Sits above the rope and translates input into concrete edits — inserting characters, deleting selections, and moving the cursor. This layer is agnostic to whether input originated from ordinary typing or a modal command; it simply applies the resulting edit to the rope.

**3. Modal input system** A state machine that tracks the current editing mode (for example, navigation mode versus insert mode) and interprets each keypress accordingly. This is what gives the editor its modal character — for instance, the same key can delete a selection in navigation mode but insert a character in insert mode.

**4. Renderer** Takes the current state of the rope, cursor position(s), and terminal dimensions, and determines what to draw: visible lines, cursor position, syntax highlighting, and line numbers. This runs on every state change and must remain efficient even on large files.

**5. Main loop** Combines the above into a standard read–update–render cycle: read input, update mode/cursor/rope state, re-render, and repeat. The collaboration layer integrates into this loop rather than replacing it.

![[edit_flow_rope_crdt_ipc.png|424]]

---
# \[Planning]
### Things to figure out
- [ ] do the [build your on text editor](https://viewsourcecode.org/snaptoken/kilo/index.html) tutorial
- [ ] figure out what tui library to use?
- [ ] how to handle utf-8 characters in the editor
- [ ] How does rope work?
- [ ] Error handling
- [ ] CLI arguments
- [ ] IPC mechanism (session)
- [ ] CRDT

### Anticipated progress
- 
### Literature work
- [diamond-type - An existing amalgamation of crdt and rope](https://github.com/josephg/diamond-types)
- [crdts go brrr - blog post by Joseph Gentle (creator of diamond-type)](https://josephg.com/blog/crdts-go-brrr/) - *may be useful*
##### Papers
- [[Replicated_abstract_data_types_Building.pdf]]
- [[fugue.pdf]]

---
# \[Implementation]
### chosen language
Since this is a system's programming problem, I choose **C**. Such a large project can teach us a lot more software engineering principles than any syllabus ever can. If we stick with C, we will understand:
- how software works beneath abstractions
- the thought process behind creating maintainable code.

Every line of code should be curated by keeping maintainability as our top priority.