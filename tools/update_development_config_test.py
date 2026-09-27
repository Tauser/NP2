import unittest

from configure_p4_development_ota import endpoints, render


class DevelopmentConfigTests(unittest.TestCase):
    def test_fixed_public_endpoints(self):
        host, urls = endpoints('https://ota.example.test/np2/public/')
        self.assertEqual(host, 'ota.example.test')
        self.assertEqual(urls[-1], 'https://ota.example.test/np2/public/candidate.bin')
        header = render('https://ota.example.test/public', bytes([1, 2, 3]), 1, 2, 7)
        self.assertIn('.key_id = 7U', header)
        self.assertNotIn('PRIVATE', header)

    def test_reject_unsafe_or_unrepresentable_origins(self):
        for url in ('http://ota.test', 'https://u:p@ota.test', 'https://ota.test:443',
                    'https://ota.test/a?token=x', 'https://ota.test/a#x',
                    'https://ota.test/../x', 'https://ota.test/%2e%2e',
                    'https://ota.test/";code', 'https://ota.test/\nx',
                    'https://bad..test', 'https://-bad.test',
                    'https://ota.test/' + 'a' * 190):
            with self.subTest(url=url), self.assertRaises(ValueError):
                endpoints(url)

    def test_identity_limits(self):
        for identity in (0, -1, 0x100000000):
            with self.assertRaises(ValueError):
                render('https://ota.test', b'x', identity, 1, 1)


if __name__ == '__main__':
    unittest.main()
