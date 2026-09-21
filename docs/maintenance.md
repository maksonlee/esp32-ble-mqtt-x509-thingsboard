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

Sampling runs in a dedicated task, not the shared ESP Timer task. Connection
events wake that task, and a sample is discarded if MQTT disconnected while it
was being acquired. MQTT enqueue keeps socket writes in the MQTT task. Its
outbox is bounded to 16 KiB; full-queue failures are logged instead of consuming
unbounded heap while acknowledgements are unavailable.

## Wi-Fi recovery

Wi-Fi event handlers never sleep to implement reconnect delays. A worker retries
saved credentials every five seconds and logs connection API errors. During BLE
enrollment the provisioning manager owns attempts, so application retries do not
race it. IP events remain registered across reconnects.
