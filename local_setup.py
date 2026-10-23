from pathlib import Path
import sys
from deployment import Deployment
from updates import Updater, read


def setup(root, source):
    updater=Updater(root)
    with updater.lock() as acquired:
        if not acquired:raise RuntimeError('Another installation is running.')
        updater.reconcile(read(updater.path))
        names=list(read(source/'INSTALL-MANIFEST.json')['files'])
        if (root/'.git').exists():raise RuntimeError('This folder is a Git checkout. Use Updates or Rebuild from Chaos HDR instead.')
        transaction=Deployment(root)
        try:
            updater.run_setup([str(root/'.venv/bin/python'),str(source/'prepare_desktop.py'),'--runtime-root',str(root),'--stage-only'])
            candidate=read(root/'output/updates/candidate.json')
            if candidate.get('release')!=read(source/'release.json') or candidate.get('development_checks') is not False:raise RuntimeError('The candidate build did not validate.')
            updater.activate(source,names,candidate)
        except Exception:
            transaction.rollback(updater.run_setup)
            raise


if __name__=='__main__':setup(Path(sys.argv[1]).resolve(),Path(sys.argv[2]).resolve())
