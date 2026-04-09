# User Commands

The user shell accepts command text and distinguishes between internal commands and external command images.

External commands resolve to `\LIBERATION\COMMANDS\*.ELF`. User command images no longer use the `X64` suffix.

`login` is an external command, installed as `\LIBERATION\COMMANDS\LOGIN.ELF`. The shell requires this command before executing non-login commands when no session is active.

Authentication success is now decided by an explicit login result from the external login command contract, not by whether the launcher managed to start the image.


### login

`login <user> <password>` is an external command resolved as `\LIBERATION\COMMANDS\LOGIN.ELF`. A login succeeds only when the password is accepted and the capability service grants the user principal `session.login`.


Authenticated shell input is normalized through the external string library at `\LIBERATION\LIBRARIES\STRING.ELF`, so commands and their arguments are uppercased before execution.
