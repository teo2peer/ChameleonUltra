def mf1_unmasked_slots(mask: bytes) -> list[int]:
    if len(mask) != 10:
        raise ValueError("MIFARE Classic key mask must contain 10 bytes")
    return [
        slot
        for slot in range(80)
        if (mask[slot // 8] & (1 << (7 - slot % 8))) == 0
    ]


def mf1_bounded_target_masks(
    mask: bytes,
    key_count: int,
    attempt_budget: int = 48,
    per_target_overhead: int = 2,
) -> list[bytearray]:
    if key_count < 1 or attempt_budget < 1 or per_target_overhead < 0:
        raise ValueError("key_count and attempt_budget must be positive")
    attempts_per_target = key_count + per_target_overhead
    if attempts_per_target > attempt_budget:
        raise ValueError("key batch exceeds the authentication attempt budget")
    targets_per_request = attempt_budget // attempts_per_target
    slots = mf1_unmasked_slots(mask)
    batches = []
    for offset in range(0, len(slots), targets_per_request):
        request_mask = bytearray(b"\xff" * 10)
        for slot in slots[offset: offset + targets_per_request]:
            request_mask[slot // 8] &= ~(1 << (7 - slot % 8))
        batches.append(request_mask)
    return batches
