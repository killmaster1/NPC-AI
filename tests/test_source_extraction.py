import unittest,tempfile,hashlib,tarfile,io,importlib.util,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('extract_sherpa',ROOT/'tools/extract_sherpa.py');extractor=importlib.util.module_from_spec(spec);spec.loader.exec_module(extractor)
class SourceExtraction(unittest.TestCase):
 def test_actual_pinned_native_sources(self):
  archive=ROOT/'third_party/sherpa/source/sherpa-v1.12.23.tar.gz';lock=json.loads((ROOT/'tts/dependencies.json').read_text())
  with tempfile.TemporaryDirectory() as temp:
   destination=Path(temp)/'source';extractor.extract(archive,destination,lock['sherpa_source_sha256'])
   self.assertTrue((destination/'CMakeLists.txt').is_file());self.assertTrue((destination/'sherpa-onnx/c-api/c-api.cc').is_file())
   self.assertTrue((destination/'sherpa-onnx/csrc/offline-tts-kokoro-impl.h').is_file())
   self.assertFalse(any(p.is_symlink() for p in destination.rglob('*')))
 def test_links_skipped_traversal_rejected(self):
  with tempfile.TemporaryDirectory() as temp:
   root=Path(temp);archive=root/'source.tar';destination=root/'extract'
   with tarfile.open(archive,'w') as out:
    link=tarfile.TarInfo('unused-example');link.type=tarfile.SYMTYPE;link.linkname='/outside';out.addfile(link)
    member=tarfile.TarInfo('../escape');member.size=1;out.addfile(member,io.BytesIO(b'x'))
   with archive.open('rb') as handle: digest=hashlib.file_digest(handle,'sha256').hexdigest()
   with self.assertRaises(tarfile.FilterError):extractor.extract(archive,destination,digest)
   self.assertFalse((root/'escape').exists());self.assertFalse((destination/'unused-example').exists())
 def test_wrong_source_digest_rejected(self):
  with tempfile.TemporaryDirectory() as temp:
   archive=Path(temp)/'bad.tar';archive.write_bytes(b'bad')
   with self.assertRaises(ValueError):extractor.extract(archive,Path(temp)/'extract','0'*64)
