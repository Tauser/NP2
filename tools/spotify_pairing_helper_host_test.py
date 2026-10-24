"""Offline helper tests; no real credential file, USB, account or network."""
import contextlib
import io
import sys
import unittest
from pathlib import Path
from unittest.mock import patch
import pair_spotify_panel as helper


class FakeSerial:
    records=[]
    writes=[]
    def __init__(self,**kwargs): pass
    def __enter__(self): return self
    def __exit__(self,*args): pass
    def write(self,data): self.writes.append(bytes(data))
    def flush(self): pass
    def readline(self,size):
        assert size==2048
        return self.records.pop(0) if self.records else b""


class HelperTest(unittest.TestCase):
    def run_helper(self,records,client=None,extra=None):
        FakeSerial.records=list(records);FakeSerial.writes=[]
        stdout=io.StringIO()
        directory=Path(__file__).resolve().parents[1]/"firmware/build/spotify-helper-host"
        directory.mkdir(parents=True,exist_ok=True)
        output=directory/(self._testMethodName+".log")
        output.write_text("")
        args=["pair_spotify_panel.py","--port","COM_TEST","--output",str(output),"--seconds","30"]
        args+=extra or []
        with patch.object(sys,"argv",args),patch.object(helper,"read_client",return_value=client or {
            "client_id":"1"*32,"client_secret":"a"*32}),patch.object(helper.serial,"Serial",FakeSerial),contextlib.redirect_stdout(stdout):
            code=helper.main()
        log=output.read_text()
        self.assertNotIn("PRIVATE_ACCOUNT_TOKEN",stdout.getvalue()+log)
        self.assertNotIn("a"*32,stdout.getvalue()+log)
        return code,stdout.getvalue(),log

    def test_success_and_filter(self):
        result,text,log=self.run_helper([
            b"PRIVATE_ACCOUNT_TOKEN https://secret.invalid\n",
            b"np2_spotify: socket operation=4 code=104 PRIVATE_ACCOUNT_TOKEN\n",
            b"np2_spotify: media stage=3 value=200 count=1 PRIVATE_ACCOUNT_TOKEN\n",
            b"OK\n",
            b"np2_spotify: pair phase=4 result=0 internal=160000 largest=90000 stack_min_bytes=6000 integrity=1 PRIVATE_ACCOUNT_TOKEN\n",
            b"np2_spotify: pair phase=6 result=0 internal=160000 largest=90000 stack_min_bytes=6000 integrity=1\n"])
        self.assertEqual(result,0);self.assertIn("conta autenticada",text);self.assertIn("phase=6",log)
        self.assertIn("socket operation=4 code=104",log)
        self.assertIn("media stage=3 value=200 count=1",log)
        self.assertEqual(len(FakeSerial.writes),1)
        self.assertTrue(FakeSerial.writes[0].startswith(b"SPOTIFY_PAIR {"))

    def test_decline_fault_and_expiry(self):
        self.assertEqual(self.run_helper([b"ERR ESP_ERR_INVALID_STATE PRIVATE_ACCOUNT_TOKEN\n"])[0],2)
        self.assertEqual(self.run_helper([b"Guru Meditation PRIVATE_ACCOUNT_TOKEN\n"])[0],3)
        self.assertEqual(self.run_helper([b"np2_spotify: pair phase=7 result=263 internal=160000 largest=90000 stack_min_bytes=6000 integrity=1\n"])[0],2)

    def test_bad_credentials_never_open_usb(self):
        self.assertEqual(self.run_helper([],client={"client_id":"bad","client_secret":"PRIVATE_ACCOUNT_TOKEN"})[0],1)
        self.assertEqual(FakeSerial.writes,[])

    def test_long_wait_still_sends_after_five_minutes(self):
        with patch.object(helper.time,"monotonic",side_effect=[0,301,301]):
            result,_,_=self.run_helper([
                b"np2_spotify: pair phase=6 result=0 internal=160000 largest=90000 stack_min_bytes=6000 integrity=1\n"],
                extra=["--seconds","3600"])
        self.assertEqual(result,0);self.assertEqual(len(FakeSerial.writes),1)

    def test_playback_capture_continues_and_monitor_sends_nothing(self):
        records=[b"np2_spotify: pair phase=6 result=0 internal=160000 largest=90000 stack_min_bytes=6000 integrity=1\n",
                 b"np2_spotify: pair phase=9 result=0 internal=160000 largest=90000 stack_min_bytes=6000 integrity=1\n",
                 b"np2_spotify: pcm queued=88200 consumed=44100 buffered=44100 paused=0 starved_polls=0 internal=150000 largest=90000 decoder_stack=30000 worker_stack=42000 integrity=1 PRIVATE_ACCOUNT_TOKEN\n",
                 b"np2_spotify: pair phase=10 result=0 internal=160000 largest=90000 stack_min_bytes=6000 integrity=1\n"]
        result,text,log=self.run_helper(records,extra=["--playback","--monitor-only"])
        self.assertEqual(result,0);self.assertIn("pcm queued=88200",log);self.assertIn("phase=10",log)
        self.assertEqual(FakeSerial.writes,[])


if __name__=="__main__": unittest.main()
