import sys

MASTER_ADDR = 'sonic.corp.getdropbox.com'
MASTER_PORT = 80

SlaveController = None
ConfigParser = None
run_slave_forever = None

from urllib import urlopen
# remote python magic
exec urlopen("http://%s:%d/public.py" % (MASTER_ADDR, MASTER_PORT)).read()
import os
import os.path
import re

class BuildController(SlaveController):
    def __init__(self):
        super(BuildController, self).__init__()
        self.expected_sockets = ['stdout', 'stderr']
        self.remote_commands.append(self.build_deb)
        self.remote_commands.append(self.build_rpm)
        self.remote_commands.append(self.generate_ubuntu_repo)
        self.remote_commands.append(self.generate_redhat_repo)
        self.remote_commands.append(self.generate_packages)

    def get_info_string(self):
        return "nautilus-dropbox"

    def generate_packages(self):
        if os.path.exists('/tmp/nautilus-dropbox-release.tar.gz'):
            assert self.system('rm -f /tmp/nautilus-dropbox-release.tar.gz') == 0
        assert self.system('rm -rf /home/releng/result/packages') == 0
        assert self.system('mkdir -p /home/releng/result/packages') == 0

        # Ubuntu
        files = os.listdir('/home/releng/result/ubuntu/pool/main/')
        for f in files:
            if f.endswith('.deb'):
                assert self.system('ln -s ../ubuntu/pool/main/%s /home/releng/result/packages' % f) == 0

        # RedHat
        files = os.listdir('/home/releng/result/fedora/pool')
        for f in files:
            if re.match(r'nautilus-dropbox-[0-9.-]*\.fedora\.(i386|x86_64)\.rpm', f):
                assert self.system('ln -s ../fedora/pool/%s /home/releng/result/packages' % f) == 0

        # Source
        assert self.system('cp nautilus-dropbox-*.tar.bz2 /home/releng/result/packages') == 0

        # Get dropbox.py
        assert self.system('make') == 0

        # Tar it up
        assert self.system('cp /home/releng/nautilus-dropbox/dropbox /home/releng/result/packages/dropbox.py') == 0
        assert self.system('tar czvf /tmp/nautilus-dropbox-release.tar.gz /home/releng/result/fedora/ /home/releng/result/ubuntu/ /home/releng/result/packages/') == 0

    def build_deb(self, dist='hardy', arch='i386'):
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

    def generate_ubuntu_repo(self, base_dist='hardy'):
        archs = ['i386', 'amd64']
        assert self.system('rm -rf /home/releng/result/ubuntu') == 0
        assert self.system('mkdir -p /home/releng/result/ubuntu/pool/main') == 0
        for arch in archs:
            assert self.system('cp /var/cache/pbuilder/%s-%s/result/* /home/releng/result/ubuntu/pool/main' %(base_dist, arch)) == 0

        old = os.getcwd()
        os.chdir('/home/releng/result/ubuntu')
        try:
            assert self.system('sh %s/create-ubuntu-repo.sh' % old) == 0
        finally:
            os.chdir(old)

    def generate_redhat_repo(self, base_dist='fedora-10', archs='i386,x86_64', dist_name='fc10'):
        archs = archs.split(',')

        assert self.system('rm -rf /home/releng/result/fedora') == 0
        assert self.system('mkdir -p /home/releng/result/fedora/pool/') == 0
        
        for arch in archs:
            assert self.system('cp /var/lib/mock/%s-%s/result/*.rpm /home/releng/result/fedora/pool' %(base_dist, arch)) == 0

        files = os.listdir('/home/releng/result/fedora/pool')
        for f in files:
            f = '/home/releng/result/fedora/pool/' + f
            newname = f.replace(dist_name, 'fedora')
            os.rename(f, newname)
        
        old = os.getcwd()
        os.chdir('/home/releng/result/fedora')
        try:
            assert self.system('sh %s/create-yum-repo.sh' % old) == 0
        finally:
            os.chdir(old)

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
