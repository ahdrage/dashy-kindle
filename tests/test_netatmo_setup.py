"""Private credential staging must validate before touching the Kindle copy."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]

class NetatmoSetupTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(); self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name)
        self.kindle=self.root/'kindle'; (self.kindle/'system').mkdir(parents=True)
        (self.kindle/'system/version.txt').write_text('Kindle 5.6.1.1 (268989035)')
        self.credentials=self.root/'credentials.json'
        self.credentials.write_text(json.dumps({'client_id':'example','client_secret':'private-test-value','refresh_token':'refresh-test','station_id':''}))

    def stage(self):
        return subprocess.run([sys.executable,str(ROOT/'tools/stage_netatmo.py'),'--credentials',str(self.credentials),'--kindle',str(self.kindle)],text=True,capture_output=True)

    def test_stages_credentials_without_putting_values_in_output(self):
        result=self.stage()
        self.assertEqual(result.returncode,0,result.stderr)
        data=json.loads((self.kindle/'dashy/netatmo.pending.json').read_text())
        self.assertEqual(data['client_secret'],'private-test-value')
        self.assertNotIn('private-test-value',result.stdout+result.stderr)
        self.assertNotIn('refresh-test',result.stdout+result.stderr)

    def test_incomplete_credentials_preserve_an_existing_pending_file(self):
        (self.kindle/'dashy').mkdir(); pending=self.kindle/'dashy/netatmo.pending.json'
        pending.write_text('preserve existing')
        self.credentials.write_text('{"client_id":"example"}')
        result=self.stage()
        self.assertNotEqual(result.returncode,0)
        self.assertEqual(pending.read_text(),'preserve existing')

    def test_wrong_firmware_is_rejected_before_staging(self):
        (self.kindle/'system/version.txt').write_text('Kindle 5.16.1')
        self.assertNotEqual(self.stage().returncode,0)
        self.assertFalse((self.kindle/'dashy').exists())

    def test_symlink_destination_does_not_write_outside_kindle(self):
        outside=self.root/'outside'; outside.mkdir()
        (self.kindle/'dashy').symlink_to(outside,target_is_directory=True)
        self.assertNotEqual(self.stage().returncode,0)
        self.assertEqual(list(outside.iterdir()),[])

if __name__=='__main__': unittest.main()
