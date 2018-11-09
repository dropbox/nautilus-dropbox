import os
import os.path
import re
import subprocess

def cmd(args):
    proc = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = proc.communicate()
    if proc.returncode != 0:
        print "Error calling %s (%d)" % (' '.join(args), proc.returncode)
        print "stdout: %s" % stdout
        print "stderr: %s" % stderr
        raise Exception("Error in call")
    return stdout.strip()

class BuildController(object):
    def system(self, args):
        proc = subprocess.Popen(args, shell=True)
        stdout, stderr = proc.communicate()
        return proc.returncode

    def generate_packages(self):
        if os.path.exists('/tmp/nautilus-dropbox-release.tar.gz'):
            assert self.system('rm -f /tmp/nautilus-dropbox-release.tar.gz') == 0

        # Source
        assert self.system('cp nautilus-dropbox-*.tar.bz2 /home/releng/result/packages') == 0

        # Get dropbox.py
        assert self.system('make dropbox') == 0
        assert self.system('cp /home/releng/nautilus-dropbox/dropbox /home/releng/result/packages/dropbox.py') == 0

        # Tar it up
        assert self.system('cd /home/releng/result/; tar czvf /tmp/nautilus-dropbox-release.tar.gz *') == 0

        rev = cmd('git describe --tags --always'.split())

        # SCP it to sonic
        assert self.system('scp /tmp/nautilus-dropbox-release.tar.gz sonic:/var/www/builds/nautilus-dropbox/nautilus-dropbox-release-%s.tar.gz' % rev) == 0

    def build_deb(self, dist, arch):
        assert self.system('sh generate-deb.sh -n') == 0
        with open('buildme') as f:
            deb_path = f.readline().strip()
        os.unlink('buildme')
        os.chdir(deb_path +'/debian')
        try:
            assert self.system('sudo DIST=%s ARCH=%s pbuilder update' % (dist, arch)) == 0
            assert self.system('sudo rm -rf /var/cache/pbuilder/%s-%s/result/*' %(dist, arch)) == 0
            assert self.system('DIST=%s ARCH=%s pdebuild' % (dist, arch)) == 0
            # auto-sign doesn't work on i386 in this version of pdebuild.  So we sign in a separate step.
            assert self.system('debsign -k5044912E /var/cache/pbuilder/%s-%s/result/*.changes' %(dist, arch)) == 0
        finally:
            os.chdir('../..')

    def generate_deb_repo(self, distro_upper, codenames, dist, archs):
        distro = distro_upper.lower()
        result = '/home/releng/result/%s' % distro
        assert self.system('rm -rf %s' % result) == 0
        assert self.system('mkdir -p %s/pool/main' % result) == 0
        for arch in archs:
            assert self.system('cp /var/cache/pbuilder/%s-%s/result/* %s/pool/main' %(dist, arch, result)) == 0

        old = os.getcwd()
        os.chdir(result)
        try:
            assert self.system('DISTS="%s" DISTRO=%s sh %s/create-ubuntu-repo.sh' %
                               (codenames, distro_upper, old)) == 0
        finally:
            os.chdir(old)

        # Add pacakge symlinks
        files = os.listdir(os.path.join(result, 'pool', 'main'))
        assert self.system('mkdir -p /home/releng/result/packages/%s' % distro) == 0
        for f in files:
            if f.endswith('.deb'):
                assert self.system('ln -s ../../%s/pool/main/%s /home/releng/result/packages/%s' %
                                   (distro, f, distro)) == 0

    def generate_yum_repo(self, distro_upper, codenames, dist, archs, dist_name):
        distro = distro_upper.lower()
        assert self.system('rm -rf /home/releng/result/fedora') == 0
        assert self.system('mkdir -p /home/releng/result/fedora/pool/') == 0

        for arch in archs:
            assert self.system('cp /var/lib/mock/%s-%s/result/*.rpm /home/releng/result/fedora/pool' %(dist, arch)) == 0

        files = os.listdir('/home/releng/result/fedora/pool')
        for f in files:
            f = '/home/releng/result/fedora/pool/' + f
            newname = f.replace(dist_name, 'fedora')
            os.rename(f, newname)

        old = os.getcwd()
        os.chdir('/home/releng/result/fedora')
        try:
            assert self.system('DISTS="%s" sh %s/create-yum-repo.sh' % (codenames, old)) == 0
        finally:
            os.chdir(old)

        # Add pacakge symlinks
        assert self.system('mkdir -p /home/releng/result/packages/%s' % distro) == 0
        files = os.listdir('/home/releng/result/fedora/pool')
        for f in files:
            if re.match(r'nautilus-dropbox-[0-9.-]*\.fedora\.(i386|x86_64)\.rpm', f):
                assert self.system('ln -s ../../%s/pool/%s /home/releng/result/packages/%s' %
                                   (distro, f, distro)) == 0

    def build_rpm(self, config):
        # config='fedora-10-i386,fedora-10-x86_64'
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

    def build_all(self):

        #self.system('git pull')
        self.system('git clean -fdx')

        info = {}
        execfile("distro-info.sh", {}, info)

        assert self.system('rm -rf /home/releng/result') == 0
        assert self.system('mkdir -p /home/releng/result/packages') == 0

        # Ubuntu
        #self.build_deb('trusty', 'i386')
        self.build_deb('trusty', 'amd64')
        self.generate_deb_repo('Ubuntu', info['UBUNTU_CODENAMES'], 'trusty', ['amd64', 'i386'])

        # Debian
        self.build_deb('jessie', 'i386')
        self.build_deb('jessie', 'amd64')
        self.generate_deb_repo('Debian', info['DEBIAN_CODENAMES'], 'jessie', ['amd64', 'i386'])

        # Fedora
        self.build_rpm('fedora-21-i386')
        self.build_rpm('fedora-21-x86_64')
        self.generate_yum_repo('Fedora', info['FEDORA_CODENAMES'], 'fedora-21', ['i386', 'x86_64'], 'fc21')

        self.generate_packages()

if __name__ == "__main__":
    BuildController().build_all()
