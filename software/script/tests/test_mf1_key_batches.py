import os
import sys
import unittest


SCRIPT_DIR = os.path.dirname(os.path.dirname(__file__))
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)

from mifare_key_batches import (  # noqa: E402
    mf1_bounded_target_masks,
    mf1_unmasked_slots,
)


class MifareClassicKeyBatchTests(unittest.TestCase):
    def test_unmasked_slots_follow_protocol_bit_order(self):
        mask = bytearray(b"\xff" * 10)
        for slot in (0, 1, 7, 8, 79):
            mask[slot // 8] &= ~(1 << (7 - slot % 8))

        self.assertEqual(mf1_unmasked_slots(mask), [0, 1, 7, 8, 79])

    def test_batches_bound_authentication_attempts(self):
        mask = bytearray(10)
        batches = mf1_bounded_target_masks(
            mask,
            key_count=12,
            attempt_budget=48,
        )

        self.assertEqual(len(batches), 27)
        self.assertTrue(all(len(mf1_unmasked_slots(batch)) <= 3 for batch in batches))
        self.assertEqual(
            sorted(slot for batch in batches for slot in mf1_unmasked_slots(batch)),
            list(range(80)),
        )

    def test_existing_masked_slots_are_not_reintroduced(self):
        mask = bytearray(b"\xff" * 10)
        for slot in (4, 5, 20):
            mask[slot // 8] &= ~(1 << (7 - slot % 8))

        batches = mf1_bounded_target_masks(mask, key_count=8, attempt_budget=16)

        self.assertEqual(
            [slot for batch in batches for slot in mf1_unmasked_slots(batch)],
            [4, 5, 20],
        )
        self.assertTrue(all(len(mf1_unmasked_slots(batch)) <= 1 for batch in batches))

    def test_key_batch_cannot_exceed_budget_by_itself(self):
        with self.assertRaises(ValueError):
            mf1_bounded_target_masks(
                bytearray(10),
                key_count=15,
                attempt_budget=16,
            )


if __name__ == "__main__":
    unittest.main()
