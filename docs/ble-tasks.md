# BLE task sync

The companion app (crosspoint-sync) adds, checks off and deletes the Home
**To-do list** over Bluetooth LE. The list lives in
`/.crosspoint/priorities.json` on the SD card; the device is the source of
truth and every reply carries the full list.

## Using it

1. Flash a build with `FREEINK_CAP_BLE_TASKS` (`pio run -e x3-tasks`). Other
   environments compile the feature out and hide the row. This build is
   peripheral-only: it leaves out the BLE page-turner host, which together with
   the peripheral role does not fit in the 6.4 MB app slot (the build enforces
   128 KB of headroom). Page-turner users stay on `x3-ble`.
2. Device: Home → **To-do list** → **Phone sync**. The device advertises while
   this screen is open and shows the **code for the app**.
3. App: **To-do list** tab → **Pairing code** → type the code once (the app
   remembers it) → **Send to device**.

The radio is on only while the Phone sync screen is open.

## The code

The code is read from `/.crosspoint/phone-sync-code.txt` on the SD card (1 to 32
letters or digits, surrounding whitespace ignored). If the file is missing or
unusable, the device writes a random six-digit one. Edit the file to choose your
own; the screen always shows the code in use.

## Security

There is **no OS-level Bluetooth pairing**: Android's pairing flow proved
unreliable for this device, so the link is plain, unencrypted GATT. The app's
first message is `{"cmd":"auth","code":"..."}` and the device refuses every
other request (answering `unauthorized` with an empty list) until the code
matches. Three wrong codes make the device hang up, and a peer that has not
authenticated within 20 seconds is dropped too. Only one phone can be connected
at a time.

What this does and does not protect: stray apps and curious neighbours cannot
read or edit the list without the code. It is **not** protection against someone
within Bluetooth range who sniffs the air: the code and the list travel in
clear. Keep anything sensitive out of the to-do list.

## GATT

| | UUID | Properties |
|---|---|---|
| Service | `6f1a0001-5c7e-4b8a-9d3e-2a4c8f1b7e01` | |
| RX (phone → device) | `6f1a0002-5c7e-4b8a-9d3e-2a4c8f1b7e01` | write, write-no-response |
| TX (device → phone) | `6f1a0003-5c7e-4b8a-9d3e-2a4c8f1b7e01` | notify, read |

The device requests ATT MTU 185. Frames are sized from the negotiated MTU
(`MTU − 3`), so a link stuck at MTU 23 still works with 20-byte frames.

## Framing

A message is a UTF-8 JSON document of at most 4096 bytes. Each write or
notification carries one frame: a header byte followed by payload.

| Header bit | Meaning |
|---|---|
| `0x01` | first frame of a message |
| `0x02` | last frame of a message |

A message that fits one frame has header `0x03`. A frame that is not `FIRST`
while no message is open, or one that would push the message past 4096 bytes,
is dropped, and the next `FIRST` frame starts clean. Reference implementations:
`src/util/TaskSync.cpp` (device) and `services/task-protocol.ts` (app).

## Requests

The first request on every connection must be `auth`:

```json
{"cmd":"auth","code":"482916"}
```

Its reply is the normal snapshot (so it doubles as the first `list`). After that:

```json
{"cmd":"list"}
{"cmd":"add","title":"Write report","note":"by noon"}
{"cmd":"toggle","id":"t1","done":true}
{"cmd":"delete","id":"t1"}
{"cmd":"set","items":[{"id":"t2","title":"Call Mom","note":"","done":false},{"title":"New task","done":true}]}
```

`set` is what the app's **Send to Device** uses: it replaces the whole list, in
the order given. Items keep their `id` when it is non-empty and unique; others
get a fresh `t<n>`. It is all-or-nothing: more than 10 items (`full`) or any
blank title (`bad_request`) changes nothing.

`title` is required (96 bytes max, `note` 120), longer values are cut on a
character boundary. Ids are assigned by the device (`t<n>`).

## Responses

Every request gets one response containing the whole list, success or not:

```json
{"v":1,"ok":true,"cap":10,"items":[{"id":"t1","title":"Write report","note":"","done":false}]}
{"v":1,"ok":false,"err":"full","cap":10,"items":[ ... ]}
```

`err` is one of `full` (10 tasks already), `not_found`, `bad_request`,
`save_failed` (SD write failed; the change is rolled back), `unauthorized` (no
successful `auth` yet) or `bad_code`. The last two always carry an empty list.

## Not covered yet

- No offline queue: edits need a live connection.
- The device does not push changes made on its own screen; the app sees them on
  the next `list`.
- No encryption (see Security).
