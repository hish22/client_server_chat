# Poll-based Chat (Client + Server)

A terminal multi-user chat application. The **server** (`server.c`) accepts
many simultaneous TCP connections using `poll()`, broadcasts messages between
clients, tracks an active user list, and keeps a short rolling history for
new joiners. The **client** (`client.c`) is a simple terminal chat UI that
lets you type and receive messages concurrently, also via `poll()`.

## Files

| File | Role |
|---|---|
| `server.c` | Multi-client TCP chat server (poll-based), broadcast + history + user list |
| `client.c` | Terminal chat client — sends/receives messages concurrently via `poll()` |

## How it works

### `server.c`
1. Creates a TCP socket, sets `SO_REUSEADDR`, binds to port **8080** on all
   interfaces, and listens (backlog 10).
2. Maintains a single `pollfd` array (`fds[MAX_CLIENTS + 1]`) with the
   listening socket at index 0 and up to `MAX_CLIENTS` (100) client sockets
   after it, plus a parallel `clients[]` array mapping fd → display name.
3. Main loop calls `poll()` once per iteration over all live descriptors:
   - **New connection** on the listening socket → `accept()`s it, adds it to
     `fds[]`, and reserves a slot in `clients[]` (rejects with "Server full!"
     if `MAX_CLIENTS` is reached).
   - **Data ready on a client socket**:
     - **First message from that client** is treated as their **name**
       (stripped of `\r\n`) rather than a chat message — the server then
       sends them the rolling chat history, broadcasts a "`<name> joined the
       chat.`" message to everyone else, and re-broadcasts the updated active
       user list.
     - **Subsequent messages** are formatted as `"<name>: <message>\n"`,
       saved into a fixed-size rolling history buffer (`save_message`, capped
       at `MAX_HISTORY` = 50, oldest entries dropped first), and broadcast to
       all other connected clients.
     - **Disconnect** (`recv` returns 0 or error) → removes the client from
       both arrays (`remove_client`, which compacts the `pollfd` array and
       decrements `nfds`), broadcasts a "`<name> left the chat.`" message, and
       re-broadcasts the updated user list.
4. `broadcast_message()` sends to every connected client except the sender
   (or to everyone if `sender_fd` is `-1`, used for system messages like join/leave/user-list).
5. `broadcast_user_list()` rebuilds and sends a formatted "Active Users" block
   to all clients whenever the roster changes.

### `client.c`
1. Prompts for a display name (`fgets`, newline stripped).
2. Connects to `SERVER_IP:PORT` (`10.61.1.118:8080`) over TCP and immediately
   sends the name as the first message (matching the server's "first message
   = name" convention).
3. Uses `poll()` to watch two file descriptors at once:
   - **stdin** — when the user types a line, it's sent to the server (blank
     lines are skipped), and the `"<name> : "` prompt is reprinted.
   - **the socket** — incoming messages are printed (with an ANSI escape
     `\r\33[2K` to clear the current input line first so incoming messages
     don't visually clash with what you're typing), then the prompt is
     reprinted.
4. Exits its loop on server disconnect (`recv` returns 0) or a socket error.

## Requirements

- Linux (or any POSIX system) with `gcc`
- No external dependencies beyond the C standard library, POSIX sockets, and `poll()`

## Build

```bash
gcc -o chat_server server.c
gcc -o chat_client client.c
```

## Run

**Start the server:**
```bash
./chat_server
```

**Connect one or more clients** (each in its own terminal, possibly on
different machines):
```bash
./chat_client
```
You'll be prompted for a display name, then dropped into the chat — type a
line and hit Enter to send it to everyone else connected.

The client is hardcoded to connect to `10.61.1.118:8080` — edit `SERVER_IP`
in `client.c` (and rebuild) to point at your actual server's IP address.

## Configuration (chat)

| Setting | Location | Default |
|---|---|---|
| Server port | `PORT` in both files | `8080` |
| Server IP (client-side) | `SERVER_IP` in `client.c` | `10.61.1.118` |
| Max simultaneous clients | `MAX_CLIENTS` in `server.c` | `100` |
| Chat history length | `MAX_HISTORY` in `server.c` | `50` messages |
| Buffer size | `BUFFER_SIZE` in both files | `1024` bytes |
| Client display-name length | `name[32]` in both files | 31 chars + NUL |

## Known limitations

- **Name = first message convention is fragile** — the server treats
  whatever arrives first on a connection as the name, with no validation;
  an empty name, a name colliding with another user, or a client that sends
  chat-shaped text first will behave unexpectedly. Duplicate names are not
  prevented.
- **No authentication or encryption** — plaintext TCP, anyone can connect and
  claim any name.
- **Fixed-size buffers throughout** (`buffer[1024]`, `name[32]`,
  `msg_out[BUFFER_SIZE + 35]`) — long messages are truncated at `recv()`
  boundaries rather than reassembled; a name near 32 chars combined with a
  long message could theoretically crowd formatting, though `snprintf` bounds
  are used for the composed message so it won't overflow the buffer itself.
- **`remove_client` index bug risk** — it linearly compacts `fds[]` by index,
  and the caller decrements the loop variable `i` afterward; this works for
  the current single-pass-per-poll structure but is easy to break if the
  polling loop is refactored (e.g. to handle multiple ready fds differently).
- **Chat history is in-memory only** — lost on server restart; no
  persistence to disk.
- **No message framing beyond newline-stripping** — messages aren't guaranteed
  to arrive in a single `recv()` call; a message split across TCP segments
  could be broadcast as two separate lines in rare cases.
- **`broadcast_user_list` string-builds unsafely with `strcat`** into a fixed
  `BUFFER_SIZE` (1024) buffer with no bounds checking — with enough
  simultaneous users and long names this could overflow.
