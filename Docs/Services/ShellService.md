# Shell Service

The shell remains a userland service and continues to accept input as shell command text.

## Internal commands

- `help`
- `version`
- `status`
- `pwd`
- `cd <path>`
- `echo <text>`
- `clear`

## External commands

Unknown commands resolve to `\LIBERATION\COMMANDS\<NAME>.ELF`.

The shell starts in a login-required state. Until a session is authenticated, the prompt is `login>` and normal commands are rejected. The required `login` command is external and resolves to `\LIBERATION\COMMANDS\LOGIN.ELF`.

Login success is not based on launcher success anymore. The shell now waits for an explicit external login verdict carried by the login command ABI. Only the `AUTHENTICATED` result flips the shell into the normal `shell>` session. A denied result keeps the shell unauthenticated.

During bootstrap, while the full exec-return path is still being wired, the runtime includes a compatibility adapter for `LOGIN.ELF` that validates the bootstrap credentials `root` / `liberation` and returns the same explicit login result the real external command path is expected to return later.


## Login policy

The shell remains login-gated until the external `login` command returns an authenticated result. In bootstrap mode, the shell login adapter consults CAPSMGR and requires the user principal to hold the `session.login` capability before the shell marks the session as authenticated.


## Command Normalization

After the user has authenticated successfully, the shell sends the full command line through the external string library at `\LIBERATION\LIBRARIES\STRING.ELF`. The current operation uppercases the entire command before builtin dispatch or external resolution. During bootstrap, the same ABI is preserved through an in-tree compatibility adapter until a dedicated library loader is available.
