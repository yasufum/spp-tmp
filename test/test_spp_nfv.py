# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Nippon Telegraph and Telephone Corporation

import configparser
import json
import requests
import time
import unittest


class TestSppNfv(unittest.TestCase):
    """Test spp_nfv.

    Test as following the REST API reference. It does not include terminating
    spp_nfv process because it is done as tearDown() task.
    """

    def setUp(self):
        """Launch default spp_nfv used for the tests."""

        self.sec_type = 'nfvs'
        self.default_sec_id = 1

        self.config = configparser.ConfigParser()
        self.config.read('config.ini')

        host = self.config['spp-ctl']['host']
        ctl_api_port = self.config['spp-ctl']['ctl_api_port']
        api_version = self.config['spp-ctl']['api_version']

        self.base_url = 'http://{host}:{port}/{api_ver}'.format(
                host=host, port=ctl_api_port, api_ver=api_version)

        # Launch default spp_nfv
        sec_port = '6666'
        nfv = {'mem': self.config['spp_nfv']['mem'],
               'lcores': self.config['spp_nfv']['lcores']}
        params = {
                'proc_name': 'spp_nfv',
                'client_id': str(self.default_sec_id),
                'eal': {
                    '-m': nfv['mem'], '-l': nfv['lcores'],
                    '--proc-type': 'secondary'},
                'app': {
                    '-s': '{}:{}'.format(host, sec_port),
                    '-n': str(self.default_sec_id)}
                }
        url = '{baseurl}/primary/launch'.format(baseurl=self.base_url)
        requests.put(url, data=json.dumps(params))
        time.sleep(0.2)  # wait until be launched

    def tearDown(self):
        """Shutdown default spp_nfv."""

        url = '{baseurl}/{sec_type}/{sec_id}'.format(
                baseurl=self.base_url,
                sec_type=self.sec_type,
                sec_id=self.default_sec_id)
        response = requests.delete(url)

    def _get_status(self):
        """Get status of default spp_nfv process."""

        url = "{baseurl}/{sec_type}/{sec_id}".format(
                baseurl=self.base_url,
                sec_type=self.sec_type,
                sec_id=self.default_sec_id)
        response = requests.get(url)
        return response.json()

    def _set_forwarding_status(self, action):
        """Set forwarding status as start or stop."""

        if action in ['start', 'stop']:
            url = "{baseurl}/{sec_type}/{sec_id}/forward".format(
                    baseurl=self.base_url,
                    sec_type=self.sec_type,
                    sec_id=self.default_sec_id)
            params = {'action': action}
            response = requests.put(url, data=json.dumps(params))
            return True
        else:
            return False

    def _add_or_del_port(self, action, res_uid):
        if action in ['add', 'del']:
            url = "{baseurl}/{sec_type}/{sec_id}/ports".format(
                    baseurl=self.base_url,
                    sec_type=self.sec_type,
                    sec_id=self.default_sec_id)
            params = {'action': action, 'port': res_uid}
            requests.put(url, data=json.dumps(params))
            return True
        else:
            return False

    def _add_port(self, res_uid):
        self._add_or_del_port('add', res_uid)

    def _del_port(self, res_uid):
        self._add_or_del_port('del', res_uid)

    def _assert_add_del_port(self, port):
        self._add_port(port)
        nfv = self._get_status()
        self.assertTrue(port in nfv['ports'])

        self._del_port(port)
        nfv = self._get_status()
        self.assertFalse(port in nfv['ports'])

    def _patch(self, src, dst):
        """Set patch between given ports."""

        url = "{baseurl}/{sec_type}/{sec_id}/patches".format(
                baseurl=self.base_url,
                sec_type=self.sec_type,
                sec_id=self.default_sec_id)
        params = {'src': src, 'dst': dst}
        requests.put(url, data=json.dumps(params))

    def _reset_patches(self):
        url = "{baseurl}/{sec_type}/{sec_id}/patches".format(
                baseurl=self.base_url,
                sec_type=self.sec_type,
                sec_id=self.default_sec_id)
        requests.delete(url)

    # Test methods for testing spp_nfv from here.
    def test_sec_id(self):
        """Confirm sec ID is expected value."""

        nfv = self._get_status()
        self.assertEqual(nfv['client-id'], 1)

    def test_forward_stop(self):
        """Confirm forwarding is started and stopped."""

        self._set_forwarding_status('start')
        nfv = self._get_status()
        self.assertEqual(nfv['status'], 'running')

        self._set_forwarding_status('stop')
        nfv = self._get_status()
        self.assertEqual(nfv['status'], 'idling')

    def test_add_del_ring(self):
        """Check if ring PMD is added."""

        port = 'ring:0'
        self._assert_add_del_port(port)

    def test_add_del_vhost(self):
        """Check if vhost PMD is added."""

        port = 'vhost:1'
        self._assert_add_del_port(port)

    def test_add_del_pcap(self):
        """Check if pcap PMD is added."""

        # TODO(yasufum): pcap cannot be adde because I do not know why...
        port = 'pcap:1'
        pass

    def test_make_patch(self):
        """Check if patch between ports is created and reseted."""

        ports = ['ring:1', 'ring:2']

        for port in ports:
            self._add_port(port)
        self._patch(ports[0], ports[1])
        nfv = self._get_status()
        self.assertTrue({'src': ports[0], 'dst': ports[1]} in nfv['patches'])

        self._reset_patches()
        nfv = self._get_status()
        self.assertEqual(nfv['patches'], [])
