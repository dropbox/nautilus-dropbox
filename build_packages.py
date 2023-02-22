import sys
import os
import os.path
import re
import subprocess

REPO_DIR = os.path.abspath(os.path.dirname(__file__))

MOCK_OUT = os.path.join(REPO_DIR, "mockout")

def cmd(args):
    proc = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = proc.communicate()
    if proc.returncode != 0:
        print("Error calling %s (%d)" % (' '.join(args), proc.returncode))
        print("stdout: %s" % stdout)
        print("stderr: %s" % stderr)
        raise Exception("Error in call")
    return stdout.strip()

class BuildController:
    def system(self, args):
        proc = subprocess.Popen(args, shell=True)
        stdout, stderr = proc.communicate()
        return proc.returncode

    def generate_packages(self):
        if os.path.exists('/tmp/nautilus-dropbox-release.tar.gz'):
            assert self.system('rm -f /tmp/nautilus-dropbox-release.tar.gz') == 0

        # Source
        assert self.system('cp nautilus-dropbox-*.tar.bz2 /tmp/dbx_build/result/packages') == 0

        # Get dropbox.py
        assert self.system('make dropbox') == 0
        assert self.system(
            'cp {REPO_DIR}/dropbox /tmp/dbx_build/result/packages/dropbox.py'.format(
                REPO_DIR=REPO_DIR,
            )) == 0

        # Tar it up
        assert self.system('cd /tmp/dbx_build/result/; tar czvf /tmp/nautilus-dropbox-release.tar.gz *') == 0

    def build_deb(self, dist, arch):
        assert self.system('sh generate-deb.sh') == 0
        with open('buildme') as f:
            deb_path = f.readline().strip()
        os.unlink('buildme')
        os.chdir(deb_path + '/debian')
        try:
            assert self.system('sudo DIST=%s ARCH=%s pbuilder update' % (dist, arch)) == 0
            assert self.system('sudo rm -rf /var/cache/pbuilder/%s-%s/result/*' % (dist, arch)) == 0
            assert self.system('DIST=%s ARCH=%s pdebuild' % (dist, arch)) == 0
            # auto-sign doesn't work on i386 in this version of pdebuild.  So we sign in a separate step.
            # We also have to use the long key ID here instead of the short one (like we do in Fedora),
            # since debsign no longer accepts short key IDs.
            assert self.system('debsign -k FC918B335044912E /var/cache/pbuilder/%s-%s/result/*.changes' % (dist, arch)) == 0
        finally:
            os.chdir('../..')

    def generate_deb_repo(self, distro_upper, codenames, dist, archs):
        distro = distro_upper.lower()
        result = '/tmp/dbx_build/result/%s' % distro
        assert self.system('rm -rf %s' % result) == 0
        assert self.system('mkdir -p %s/pool/main' % result) == 0
        for arch in archs:
            assert self.system('cp /var/cache/pbuilder/%s-%s/result/* %s/pool/main' % (dist, arch, result)) == 0

        old = os.getcwd()
        os.chdir(result)
        try:
            assert self.system('DISTS="%s" DISTRO=%s sh %s/create-ubuntu-repo.sh' %
                               (codenames, distro_upper, old)) == 0
        finally:
            os.chdir(old)

        # Add pacakge symlinks
        files = os.listdir(os.path.join(result, 'pool', 'main'))
        assert self.system('mkdir -p /tmp/dbx_build/result/packages/%s' % distro) == 0
        for f in files:
            if f.endswith('.deb'):
                assert self.system('ln -s ../../%s/pool/main/%s /tmp/dbx_build/result/packages/%s' %
                                   (distro, f, distro)) == 0

    def generate_yum_repo(self, distro_upper, codenames, dist, archs, dist_name):
        distro = distro_upper.lower()
        assert self.system('rm -rf /tmp/dbx_build/result/fedora') == 0
        assert self.system('mkdir -p /tmp/dbx_build/result/fedora/pool/') == 0

        for arch in archs:
            assert self.system('cp %s/%s-%s/result/*.rpm /tmp/dbx_build/result/fedora/pool' % (MOCK_OUT, dist, arch)) == 0

        files = os.listdir('/tmp/dbx_build/result/fedora/pool')
        for f in files:
            f = '/tmp/dbx_build/result/fedora/pool/' + f
            newname = f.replace(dist_name, 'fedora')
            os.rename(f, newname)

        old = os.getcwd()
        os.chdir('/tmp/dbx_build/result/fedora')
        try:
            assert self.system('DISTS="%s" sh %s/create-yum-repo.sh' % (codenames, old)) == 0
        finally:
            os.chdir(old)

        # Add pacakge symlinks
        assert self.system('mkdir -p /tmp/dbx_build/result/packages/%s' % distro) == 0
        files = os.listdir('/tmp/dbx_build/result/fedora/pool')
        for f in files:
            if re.match(r'nautilus-dropbox-[0-9.-]*\.fedora\.(i386|x86_64)\.rpm', f):
                assert self.system('ln -s ../../%s/pool/%s /tmp/dbx_build/result/packages/%s' %
                                   (distro, f, distro)) == 0

    def build_rpm(self, config):
        # config='fedora-37-i386,fedora-37-x86_64'
        assert self.system('sh generate-rpm.sh') == 0

        path = None
        with open('buildme') as f:
            for l in f.readlines():
                if l.startswith('Wrote: '):
                    path = l[7:]
                    break
        os.unlink('buildme')

        if not path:
            raise Exception("No srpm generated")

        assert ' ' not in MOCK_OUT, "MOCK_OUT path cannot contain spaces: %s" % (MOCK_OUT,)

        mock_config_out = os.path.join(MOCK_OUT, config, "result")

        assert self.system('rm -rf %s/*' % (mock_config_out,)) == 0

        try:
            assert self.system(
                'sudo /usr/bin/mock -r {config} --resultdir={mock_config_out} rebuild {path}'.format(
                    config=config,
                    mock_config_out=mock_config_out,
                    path=path)) == 0
        except Exception:
            # We failed.  Let's push the logs to the server so that we have them.
            self.system('cat %s/*.log >&2' % (mock_config_out,))
            raise
        else:
            username = cmd('whoami').decode('utf-8')
            assert self.system('sudo chown -R %s:%s %s' % (username, username, mock_config_out)) == 0
            assert self.system('find %s/ -name *.rpm -exec /usr/bin/expect sign-rpm.exp {} \\;' % (
                mock_config_out,)) == 0

    def build_all(self):

        self.system('git pull')
        self.system('git clean -fdx')

        info = {}
        with open("distro-info.sh") as f:
            exec(f.read(), {}, info)

        assert self.system('rm -rf /tmp/dbx_build/result') == 0
        assert self.system('mkdir -p /tmp/dbx_build/result/packages') == 0

        if len(sys.argv) == 1 or 'deb' in sys.argv:
            # Ubuntu
            self.build_deb('kinetic', 'amd64')
            self.generate_deb_repo('Ubuntu', info['UBUNTU_CODENAMES'], 'kinetic', ['amd64'])

            # Debian
            self.build_deb('bookworm', 'i386')
            self.build_deb('bookworm', 'amd64')
            self.generate_deb_repo('Debian', info['DEBIAN_CODENAMES'], 'bookworm', ['amd64', 'i386'])

        # Fedora
        if len(sys.argv) == 1 or 'rpm' in sys.argv:
            # Fedora dropped 32-bit support several versions ago.
            self.build_rpm('fedora-37-x86_64')
            self.generate_yum_repo('Fedora', info['FEDORA_CODENAMES'], 'fedora-37', ['x86_64'], 'fc37')

        if len(sys.argv) == 1 or 'package' in sys.argv:
            self.generate_packages()


if __name__ == "__main__":
    assert sys.version_info[0] >= 3, "Must be running in Python 3"
    BuildController().build_all()
