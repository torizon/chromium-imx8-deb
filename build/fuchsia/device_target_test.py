#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests scenarios with number of devices and invalid devices"""
import subprocess
import unittest
import unittest.mock as mock
from argparse import Namespace
from device_target import DeviceTarget
from ffx_session import FfxRunner, FfxTarget
from target import Target, FuchsiaTargetException


class TestDiscoverDeviceTarget(unittest.TestCase):
  def setUp(self):
    self.args = Namespace(out_dir='out/fuchsia',
                          target_cpu='x64',
                          host=None,
                          node_name=None,
                          port=None,
                          ssh_config='mock_config',
                          fuchsia_out_dir=None,
                          os_check='ignore',
                          logs_dir=None,
                          system_image_dir=None)

  def testNoNodeNameOneDeviceReturnNoneCheckNameAndAddress(self):
    with DeviceTarget.CreateFromArgs(self.args) as device_target_instance, \
         mock.patch.object(FfxRunner, 'list_targets') as mock_list_targets, \
         mock.patch.object(
             FfxTarget, 'get_ssh_address') as mock_get_ssh_address, \
         mock.patch.object(
             DeviceTarget, '_ConnectToTarget') as mock_connecttotarget:
      mock_list_targets.return_value = [{
          "nodename": "device_name",
          "rcs_state": "Y",
          "serial": "<unknown>",
          "target_type": "terminal.qemu-x64",
          "target_state": "Product",
          "addresses": ["address"]
      }]
      mock_get_ssh_address.return_value = ('address', 12345)
      mock_connecttotarget.return_value = True
      self.assertIsNone(device_target_instance.Start())
      self.assertEqual(device_target_instance._node_name, 'device_name')
      self.assertEqual(device_target_instance._host, 'address')
      self.assertEqual(device_target_instance._port, 12345)

  def testNoNodeNameTwoDevicesRaiseExceptionAmbiguousTarget(self):
    with DeviceTarget.CreateFromArgs(self.args) as device_target_instance, \
         mock.patch.object(FfxRunner, 'list_targets') as mock_list_targets, \
         self.assertRaisesRegex(Exception, \
                                'More than one device was discovered'):
      mock_list_targets.return_value = [{
          "nodename": "device_name1",
          "rcs_state": "Y",
          "serial": "<unknown>",
          "target_type": "terminal.qemu-x64",
          "target_state": "Product",
          "addresses": ["address1"]
      }, {
          "nodename": "device_name2",
          "rcs_state": "Y",
          "serial": "<unknown>",
          "target_type": "terminal.qemu-x64",
          "target_state": "Product",
          "addresses": ["address2"]
      }]
      device_target_instance.Start()
      self.assertIsNone(device_target_instance._node_name)
      self.assertIsNone(device_target_instance._host)

  def testNoNodeNameDeviceDoesntHaveNameRaiseExceptionCouldNotFind(self):
    with DeviceTarget.CreateFromArgs(self.args) as device_target_instance, \
         mock.patch.object(FfxRunner, 'list_targets') as mock_list_targets, \
         self.assertRaisesRegex(Exception, 'Could not find device.'):
      mock_list_targets.return_value = [{
          "nodename": "<unknown>",
          "rcs_state": "Y",
          "serial": "<unknown>",
          "target_type": "terminal.qemu-x64",
          "target_state": "Product",
          "addresses": ["address"]
      }]
      device_target_instance.Start()
      self.assertIsNone(device_target_instance._node_name)
      self.assertIsNone(device_target_instance._host)

  def testNodeNameDefinedDeviceFoundReturnNoneCheckNameAndHost(self):
    self.args.node_name = 'device_name'
    with DeviceTarget.CreateFromArgs(self.args) as device_target_instance, \
         mock.patch.object(
             FfxTarget, 'get_ssh_address') as mock_get_ssh_address, \
         mock.patch.object(
             DeviceTarget, '_ConnectToTarget') as mock_connecttotarget:
      mock_get_ssh_address.return_value = ('address', 12345)
      mock_connecttotarget.return_value = True
      self.assertIsNone(device_target_instance.Start())
      self.assertEqual(device_target_instance._node_name, 'device_name')
      self.assertEqual(device_target_instance._host, 'address')
      self.assertEqual(device_target_instance._port, 12345)

  def testNodeNameDefinedDeviceNotFoundRaiseExceptionCouldNotFind(self):
    self.args.node_name = 'wrong_device_name'
    with DeviceTarget.CreateFromArgs(self.args) as device_target_instance, \
         mock.patch.object(
             FfxTarget, 'get_ssh_address') as mock_get_ssh_address, \
         self.assertRaisesRegex(Exception, 'Could not find device.'):
      mock_get_ssh_address.side_effect = subprocess.CalledProcessError(
          1, 'ffx', 'Timeout attempting to reach target "wrong_device_name"')
      device_target_instance.Start()
      self.assertIsNone(device_target_instance._node_name)
      self.assertIsNone(device_target_instance._host)

  def testNoDevicesFoundRaiseExceptionCouldNotFind(self):
    with DeviceTarget.CreateFromArgs(self.args) as device_target_instance, \
         mock.patch.object(FfxRunner, 'list_targets') as mock_list_targets, \
         self.assertRaisesRegex(Exception, 'Could not find device.'):
      mock_list_targets.return_value = []
      device_target_instance.Start()
      self.assertIsNone(device_target_instance._node_name)
      self.assertIsNone(device_target_instance._host)

  def testNoProvisionDeviceIfVersionsMatch(self):
    self.args.os_check = 'update'
    self.args.system_image_dir = 'mockdir'
    with DeviceTarget.CreateFromArgs(self.args) as device_target_instance, \
         mock.patch.object(DeviceTarget, '_Discover') as mock_discover, \
         mock.patch.object(DeviceTarget, '_ConnectToTarget') as mock_connect, \
         mock.patch.object(DeviceTarget, '_GetSdkHash') as mock_hash, \
         mock.patch.object(
            DeviceTarget, '_GetInstalledSdkVersion') as mock_version, \
         mock.patch.object(DeviceTarget, '_ProvisionDevice') as mock_provision:
      mock_discover.return_value = True
      mock_hash.return_value = '1.0'
      mock_version.return_value = '1.0'
      device_target_instance.Start()
      self.assertEqual(mock_provision.call_count, 0)

  def testRaiseExceptionIfCheckVersionsNoMatch(self):
    self.args.os_check = 'check'
    self.args.system_image_dir = 'mockdir'
    with DeviceTarget.CreateFromArgs(self.args) as device_target_instance, \
         mock.patch.object(DeviceTarget, '_Discover') as mock_discover, \
         mock.patch.object(DeviceTarget, '_ConnectToTarget') as mock_ready, \
         mock.patch.object(DeviceTarget, '_GetSdkHash') as mock_hash, \
         mock.patch.object(
            DeviceTarget, '_GetInstalledSdkVersion') as mock_version, \
         mock.patch.object(
            DeviceTarget, '_ProvisionDevice') as mock_provision, \
         self.assertRaisesRegex(Exception, 'Image and Fuchsia version'):
      mock_discover.return_value = True
      mock_hash.return_value = '2.0'
      mock_version.return_value = '1.0'
      device_target_instance.Start()

  def testProvisionIfOneNonDetectableDevice(self):
    self.args.os_check = 'update'
    self.args.node_name = 'mocknode'
    self.args.system_image_dir = 'mockdir'
    with DeviceTarget.CreateFromArgs(self.args) as device_target_instance, \
         mock.patch.object(
             FfxTarget, 'get_ssh_address') as mock_get_ssh_address, \
         mock.patch.object(DeviceTarget, '_ProvisionDevice') as mock_provision:
      mock_get_ssh_address.side_effect = subprocess.CalledProcessError(
          1, 'ffx', 'Timeout attempting to reach target "mocknode"')
      device_target_instance.Start()
      self.assertEqual(mock_provision.call_count, 1)


if __name__ == '__main__':
  unittest.main()
