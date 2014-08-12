#LIRC ideas of code

So, you are considering contributing to lirc? You are most welcome! Some points:

## Code layout

We use the kernel coding standards, as described in [1]. Many lines in the code
are actually too long according to these, but stick to the rules for new code.

In the git-tools directory there is a pre-commit hook aimed to be installed in
.git/hooks/pre-commit (a symlink works fine). This handles most of the boring
tabs and trailing whitespace problems on checkin. Here is also a
fix-whitespace script which can be used to filter a range of commits.

## Git branches

We basically use the branching scheme described in [2]. However, what is called
'devel' in that document we call 'master'. Likewise, what is called 'master' in [2]
we call 'release'. In short:

    - master is the current development, from time to time unstable.
    - release contains the last stable version, and also all tagged releases.
    - When a release is upcoming, we fork a release branch. This is kept
      stable, only bugfixes are allowed. Eventually it is merged into release
      and tagged.
    - Other branches are feature branches for test and review. They can not
      be trusted, and are often rewritten.

## New remote configuration files

There is a document contrib/remote-checklist.txt describing how to submit
new remotes.

## Testing and and bug reporting

Non-trivial changes should be checked using the lirc-codecs-regression-test.sh
in contrib.  New testing tools are more than welcome...

Please report bugs, RFE.s etc in sourceforge [3]; either the mailing list or the
issue tracker.

[1] https://www.kernel.org/doc/Documentation/CodingStyle
[2] http://nvie.com/posts/a-successful-git-branching-model
[3] http://sourceforge.net/projects/lirc/
