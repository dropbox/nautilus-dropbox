import sys

MASTER_ADDR = 'chaos.corp.getdropbox.com'
MASTER_PORT = 8080

SlaveController = None
ConfigParser = None
run_slave_forever = None

from urllib import urlopen
# remote python magic
exec urlopen("http://%s:%d/public.py" % (MASTER_ADDR, MASTER_PORT)).read()
import os

class BuildController(SlaveController):
    def __init__(self):
        super(BuildController, self).__init__()
        self.expected_sockets = ['stdout', 'stderr']
        self.remote_commands.append(self.build_deb)
        self.remote_commands.append(self.build_rpm)

    def get_info_string(self):
        return "nautilus-dropbox"

    def build_deb(self, dist='karmic', arch='i386'):
        # dist = 'karmic', arch = {i386, amd64}
        assert self.system('sh generate-deb.sh -n') == 0
        with open('buildme') as f:
            deb_path = f.readline().strip()
        os.unlink('buildme')
        os.chdir(deb_path +'/debian')
        try:
            assert self.system('sudo DIST=%s ARCH=%s pbuilder update' % (dist, arch)) == 0
            assert self.system('rm -rf /var/cache/pbuilder/%s-%s/result/*' %(dist, arch)) == 0
            assert self.system('DIST=%s ARCH=%s pdebuild' % (dist, arch)) == 0
            # auto-sign doesn't work on i386 in this version of pdebuild.  So we sign in a separate step.
            assert self.system('debsign -k5044912E /var/cache/pbuilder/%s-%s/result/*.changes' %(dist, arch)) == 0
        finally:
            os.chdir('../..')

    def build_rpm(self, config='fedora-10-i386'):
        # fcdists='fedora-10-i386,fedora-10-x86_64'
        assert self.system('sh generate-rpm.sh -n') == 0

        path = None
        with open('buildme') as f:
            for l in f.readlines():
                if l.startswith('Wrote: '):
                    path = l[7:]
                    break
        os.unlink('buildme')

        if not path:
            raise Exception("No srpm generated")

        assert self.system('rm -rf /var/lib/mock/%s/result/*' %(config)) == 0

        try:
            assert self.system('/usr/bin/mock -r %s rebuild %s' % (config, path)) == 0
        except:
            # We failed.  Let's push the logs to the server so that we have them.
            self.system('cat /var/lib/mock/%s/*.log >&2' %(config,))
            raise
        else:
            assert self.system('find /var/lib/mock/%s/result/ -name *.rpm | xargs /usr/bin/expect sign-rpm.exp' %(config,)) == 0

# self.system('hg pull -u')
# self.system('hg purge')

if __name__ == "__main__":
    run_slave_forever(BuildController)
