# English launcher — 0.5.29

All application-owned launcher headings, buttons, graphics descriptions,
driver choices, dialogs, ready/missing-file states, verification progress and
error messages are English. The source/driver validators use English errors
so their messages cannot reintroduce Indonesian into launcher dialogs.

MainActivity uses an English configuration context, including built-in widget
accessibility text. This does not change the phone language or the system
document picker. User-selected filenames and driver ZIP names remain intact.
Game startup display messages and the session notification are also English;
native event identifiers remain unchanged.

The branding, icons, launcher arrangement, native runtime and stored settings
are preserved. Only Java text/context and APK version change. No test APKs
are installed and no gameplay/debug session is started during this update.

In 0.5.30, the Game Files card (Select EXE and Select folder) moves directly
below Play Game, before Graphics, GPU Driver and Display. Its selection and
verification behavior is unchanged, and all launcher text remains English.
