# Reliability and maintenance

## Certificate storage

Certificate files must be nonempty and no larger than 16 KiB each. The loader
rejects seek failures, invalid sizes, short reads, and stream errors. A failed
mount never formats the certificate partition. Mount and cleanup operations
are idempotent, and released private-key buffers are wiped.

Run host fault-injection tests through the host build limiter:

```bash
/home/administrator/.local/bin/codex-build-limited -- \
  python3 -m unittest discover -s tests -p test_native.py -v
```

These tests compile the actual file loader with failing I/O substitutes and
AddressSanitizer/UndefinedBehaviorSanitizer. They do not access device secrets.

## MQTT diagnostics

Submission and broker acknowledgement are separate log events. Negative publish
results are failures, not successful uploads. A QoS 1 acknowledgement confirms
broker receipt; verify ThingsBoard telemetry separately for application receipt.
Transport failures include TLS verification flags, TLS errors, socket errno,
and CONNACK status without printing keys or credentials.
