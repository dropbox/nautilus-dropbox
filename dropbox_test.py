#!/usr/bin/env python3
#
# Copyright (c) Dropbox, Inc.
#
# dropbox_test.py
# unit tests for Dropbox frontend script
#
# This file is part of nautilus-dropbox.
#
# nautilus-dropbox is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# nautilus-dropbox is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with nautilus-dropbox.  If not, see <http://www.gnu.org/licenses/>.
#

import os
import shutil
import socket
import sys
import unittest
from importlib.util import spec_from_loader, module_from_spec
from importlib.machinery import SourceFileLoader
from unittest.mock import MagicMock

spec = spec_from_loader("dropbox", SourceFileLoader("dropbox", "./dropbox"))
dropbox = module_from_spec(spec)
if spec.loader:
    spec.loader.exec_module(dropbox)

class TestDropbox(unittest.TestCase):

    def test_plat_fails_on_non_linux(self):
        sys.platform = 'darwin'
        #with self.assertRaises(dropbox.FatalVisibleError):
        dropbox.plat()

    def test_reroll_autostart_without_config_dir(self):
        os.listdir = MagicMock(return_value=[])
        os.makedirs = MagicMock()
        os.remove = MagicMock()
        dropbox.reroll_autostart(False)
        os.makedirs.assert_not_called()
        os.remove.assert_not_called()
        dropbox.reroll_autostart(True)
        os.makedirs.assert_not_called()
        os.remove.assert_not_called()

    def test_reroll_autostart_false(self):
        os.listdir = MagicMock(return_value=['.config'])
        os.path.exists = MagicMock(return_value=False)
        os.makedirs = MagicMock()
        os.remove = MagicMock()
        dropbox.reroll_autostart(False)
        os.makedirs.assert_not_called()
        os.remove.assert_not_called()
        os.path.exists.return_value = True
        dropbox.reroll_autostart(False)
        os.makedirs.assert_not_called()
        os.remove.assert_called_once()

    def test_reroll_autostart_true(self):
        os.listdir = MagicMock(return_value=['.config'])
        os.path.exists = MagicMock(side_effect=[True, False])
        shutil.copyfile = MagicMock()
        os.makedirs = MagicMock()
        os.remove = MagicMock()
        dropbox.reroll_autostart(True)
        os.makedirs.assert_called_once()
        shutil.copyfile.assert_called_once()
        os.remove.assert_not_called()
        os.path.exists.return_value = True


class TestDropboxCommand(unittest.TestCase):
    class MockFile:
        def __init__(self):
            self.buf = ""
        
        def write(self, text):
            self.buf += text
        
        def writelines(self, texts):
            for text in texts:
                self.buf += text
        
    def setUp(self):
        # Set all mocks for DropboxCommand
        self.mock_socket = MagicMock()
        self.mock_file = TestDropboxCommand.MockFile()
        self.mock_socket.makefile = MagicMock(return_value=self.mock_file)
        socket.socket = MagicMock(return_value=self.mock_socket)

        # Create DropboxCommand with mocks
        self.cmd = dropbox.DropboxCommand()

        # Verify dropboxCommand constructor's behavior
        self.mock_socket.connect.assert_called_once()
        self.mock_socket.makefile.asssert_called_once()

        # Set common mocks for the command calls
        self.mock_ticker = MagicMock()
        dropbox.CommandTicker = self.mock_ticker

        # Set return ok - success for the default command response
        self.mock_file.readline = MagicMock(side_effect=['ok', 'done'])

        self.mock_file.flush = MagicMock()
        self.mock_file.close = MagicMock()
        self.mock_socket.close = MagicMock()

    def tearDown(self):
        self.mock_file.flush.assert_called_once()

        # Verify DropboxCommand.close correctly close the file and the socket.
        self.cmd.close()
        self.mock_file.close.assert_called_once()
        self.mock_socket.close.assert_called_once()
        
    def test_command_without_param(self):
        self.cmd.get_dropbox_status()
        assert self.mock_file.buf == "get_dropbox_status\ndone\n"
    
    def test_command_with_params(self):
        kwargs = {
            'download_mode': 'manual',
            'upload_mode': 'manual',
            'download_limit': '640',
            'upload_limit': '128',
        }
        self.cmd.set_bandwidth_limits(**kwargs)
        assert self.mock_file.buf == ('set_bandwidth_limits\n' +
                                    'download_mode\tmanual\n' +
                                    'upload_mode\tmanual\n' +
                                    'download_limit\t640\n' +
                                    'upload_limit\t128\n' +
                                    'done\n')

    def test_command_error(self):
        self.mock_file.readline = MagicMock(side_effect=['error', 'command not available', 'done'])
        with self.assertRaises(dropbox.DropboxCommand.CommandError): 
            self.cmd.get_dropbox_status()


if __name__ == '__main__':
    unittest.main()
