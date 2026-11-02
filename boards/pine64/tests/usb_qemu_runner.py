"""Drive one real emulated USB keyboard report through TinyUSB/OHCI."""
import json
import os
from pathlib import Path
import subprocess
import sys
import time

out = Path(sys.argv[1])


def main():
    serial_path = out / "usb-qemu.log"
    serial_path.write_bytes(b"")
    command = [os.environ.get("QEMU_AARCH64", "qemu-system-aarch64"),
               "-machine", "virt,virtualization=on,gic-version=2,highmem=off",
               "-cpu", "cortex-a53", "-smp", "1", "-m", "2G",
               "-display", "none", "-monitor", "none", "-serial", f"file:{serial_path}",
               "-nic", "none", "-no-reboot", "-semihosting-config", "enable=on,target=native",
               "-qmp", "stdio",
               "-device", "pci-ohci,id=ohci,addr=0x1,num-ports=1",
               "-device", "usb-kbd,bus=ohci.0,port=1",
               "-device", f"loader,file={out / 'usb-host.bin'},addr=0x41000000,force-raw=on",
               "-device", f"loader,file={out / 'enter32.elf'},cpu-num=0"]
    proc = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, bufsize=0)
    transcript = bytearray()
    try:
        deadline = time.monotonic() + 25
        stream = proc.stdout
        json.loads(stream.readline())

        def execute(name, arguments=None):
            message = {"execute": name}
            if arguments is not None:
                message["arguments"] = arguments
            proc.stdin.write(json.dumps(message).encode() + b"\n")
            while True:
                response = json.loads(stream.readline())
                if "error" in response:
                    raise RuntimeError(response["error"])
                if "return" in response:
                    return response["return"]

        execute("qmp_capabilities")
        injected = False
        while time.monotonic() < deadline:
            current = serial_path.read_bytes()
            chunk = current[len(transcript):]
            transcript.extend(chunk)
            sys.stdout.buffer.write(chunk)
            sys.stdout.buffer.flush()
            if b"USB QEMU: keyboard enumerated" in transcript and not injected:
                execute("send-key", {"keys": [{"type": "qcode", "data": "w"},
                                               {"type": "qcode", "data": "z"}],
                                     "hold-time": 150})
                injected = True
            if proc.poll() is not None:
                break
            time.sleep(0.02)
        else:
            raise TimeoutError("USB integration test exceeded 25 seconds")
        status = proc.wait(timeout=2)
        if status != 0 or b"USB QEMU PASS:" not in transcript:
            raise RuntimeError(f"USB integration test failed (QEMU exit {status})")
    finally:
        if proc.poll() is None:
            proc.terminate()
            proc.wait(timeout=3)
        errors = proc.stderr.read()
        if errors:
            sys.stderr.buffer.write(errors)


if __name__ == "__main__":
    main()
