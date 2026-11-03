"""Essais de mise à jour hors réseau, dans des clones temporaires uniquement."""
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile
import time

driver = str(Path(sys.argv[1]).resolve())
env = dict(os.environ, GIT_CONFIG_GLOBAL=os.devnull, GIT_CONFIG_NOSYSTEM="1")


def run(*args, cwd=None):
    return subprocess.run(args, cwd=cwd, env=env, check=True, capture_output=True, text=True).stdout


def git(directory, *args):
    return run("git", "-C", str(directory), *args)


def commit(directory, message):
    git(directory, "add", ".")
    git(directory, "-c", "user.name=Test", "-c", "user.email=test@example.invalid", "commit", "-qm", message)


with tempfile.TemporaryDirectory(prefix="fastfps-updater-") as temp:
    root = Path(temp)
    source = root / "source"
    source.mkdir()
    git(source, "init", "-q", "-b", "main")
    (source / "Makefile").write_text("all:\n\t@cp payload build/fastfps\n\t@chmod +x build/fastfps\n")
    (source / "payload").write_text("#!/bin/sh\nexit 0\n")
    # Le lanceur ne démarre aucun jeu pendant le test.
    (source / "lancer.sh").write_text('echo relance > "$(dirname "$0")/relance.txt"\nexit 37\n')
    (source / ".gitignore").write_text("build/\nrelance.txt\n")
    commit(source, "base")

    def clone(name):
        path = root / name
        run("git", "clone", "-q", str(source), str(path))
        (path / "build").mkdir()
        return path

    good = clone("clone avec espace et ' apostrophe")
    (source / "nouveau.txt").write_text("nouvelle version\n")
    commit(source, "mise a jour")
    local_driver = good / "build" / "test-updates"
    shutil.copy2(driver, local_driver)
    print(run(str(local_driver), "AUTO", "success"), end="")
    assert (good / "nouveau.txt").exists() and (good / "relance.txt").exists()
    assert git(good, "rev-parse", "HEAD") == git(source, "rev-parse", "HEAD")
    closing = clone("fermeture-apres-compilation")
    print(run(driver, str(closing), "close"), end="")
    assert not (closing / "relance.txt").exists()

    dirty = clone("modifications")
    (dirty / "payload").write_text("modification personnelle\n")
    print(run(driver, str(dirty), "MODIFICATIONS LOCALES"), end="")
    assert (dirty / "payload").read_text() == "modification personnelle\n"
    assert not (dirty / "relance.txt").exists()

    branch = clone("autre-branche")
    git(branch, "checkout", "-qb", "travail")
    print(run(driver, str(branch), "BRANCHE MAIN"), end="")

    diverged = clone("divergence")
    (diverged / "local.txt").write_text("local\n")
    commit(diverged, "local")
    (source / "distant.txt").write_text("distant\n")
    commit(source, "distant")
    head = git(diverged, "rev-parse", "HEAD")
    print(run(driver, str(diverged), "ECHEC GIT"), end="")
    assert git(diverged, "rev-parse", "HEAD") == head
    assert not (diverged / "relance.txt").exists()

    broken = clone("compilation-ratee")
    (broken / "Makefile").write_text("all:\n\t@exit 1\n")
    commit(broken, "compilation invalide")
    # Avancer aussi la source afin que le pull soit une avance rapide.
    git(source, "fetch", str(broken), "main")
    git(source, "merge", "--ff-only", "FETCH_HEAD")
    print(run(driver, str(broken), "ECHEC DE COMPILATION"), end="")
    assert not (broken / "relance.txt").exists()
    slow = clone("annulation")
    (slow / "Makefile").write_text("all:\n\t@sleep 2; touch processus-abandonne.txt; sleep 60\n")
    commit(slow, "compilation lente")
    git(source, "fetch", str(slow), "main")
    git(source, "merge", "--ff-only", "FETCH_HEAD")
    print(run(driver, str(slow), "cancel"), end="")
    time.sleep(2.2)
    assert not (slow / "processus-abandonne.txt").exists()
    run(driver, str(root), "invalid")
print("OK : mise a jour locale, chemins speciaux, modifications, branche, divergence et compilation")
