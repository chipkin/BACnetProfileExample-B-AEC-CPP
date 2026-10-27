#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
# Public-domain example code (CC0) - see ../LICENSE.
"""B-AEC Wire Test - the Peer backs the configuration up, changes it, and restores it (DM-BR-B).

    python3 tests/backup_restore_test.py [--exe build/BACnetExampleBAEC] [--port 47834]

135-2024 L.13.1: "Ability to upload its configuration and allow it to be subsequently
restored". The procedure is cl. 19.1: ReinitializeDevice START_BACKUP, read the files the
Device's Configuration_Files names with AtomicReadFile, END_BACKUP; later START_RESTORE,
AtomicWriteFile the same content back, END_RESTORE. The configuration this controller
holds is the set of Analog Values another device created and their Present_Values
(main.cpp, "THE CONFIGURATION IMAGE"). The controller runs with --dcc-password, which
ReinitializeDevice must carry.

  1. Setup: CreateObject Analog Value 100 = 42.5 and Analog Value 101 = 1.0.
  2. FAILURE MODE: START_BACKUP with a wrong password -> SECURITY / PASSWORD_FAILURE, and
     System_Status stays OPERATIONAL.
  3. START_BACKUP -> SimpleACK; System_Status BACKUP_IN_PROGRESS; Configuration_Files names
     File 1; AtomicReadFile of File 1 returns the image, which names Analog Value 100 at
     42.5; END_BACKUP -> SimpleACK, System_Status OPERATIONAL.
  4. The changed setting: Analog Value 100 written to 7.0, Analog Value 101 deleted,
     Analog Value 102 created.
  5. START_RESTORE -> SimpleACK; WriteProperty File 1 File_Size = 0 -> SimpleACK and it
     reads 0 (K.5.18; FAILURE MODE: a size over 4096 -> PROPERTY / VALUE_OUT_OF_RANGE);
     AtomicWriteFile of the backed-up image -> ACK, File_Size reads its size; control:
     before END_RESTORE nothing has changed (100 still reads 7.0); END_RESTORE -> SimpleACK.
  6. The configuration is the backed-up one again: 100 reads 42.5, 101 exists with 1.0,
     102 is gone; Last_Restore_Time is set.
  7. FAILURE MODE: after 100 is changed to 9.0, a restore of a file that is not a
     configuration image changes nothing (100 still 9.0), and Last_Restore_Time does not move.
"""
import sys
import time

import aec_setup as setup
from aec_setup import services, wiretest
from wiretest import PROPERTY_PRESENT_VALUE, record

ANALOG_VALUE = setup.OBJECT_TYPE_ANALOG_VALUE
SETTING = (ANALOG_VALUE, 100)
SECOND = (ANALOG_VALUE, 101)
LATER = (ANALOG_VALUE, 102)
PROPERTY_CONFIGURATION_FILES = 154
PROPERTY_FILE_SIZE = 42
FILE_MAX_SIZE = 4096                  # main.cpp: Ivory holds at most 4096 octets
STEP_WAIT = 3.0


def reinitialize(workstation, args, state, password=setup.PASSWORD):
    return workstation.request(lambda invoke_id: wiretest.reinitialize_device_request(invoke_id, state, password),
                               args.port)


def system_status(workstation, args):
    return setup.read(workstation, args, setup.DEVICE, setup.PROPERTY_SYSTEM_STATUS)


def create(workstation, args, reference, value):
    created = workstation.request(lambda invoke_id: services.create_object_request(invoke_id, *reference), args.port)
    written = workstation.write_property(args.port, *reference, PROPERTY_PRESENT_VALUE, wiretest.application_real(value))
    return created.ok and written.ok


def present_value(workstation, args, reference):
    answer = workstation.read_property(args.port, *reference, PROPERTY_PRESENT_VALUE)
    return answer.value if answer.ok else answer.error


def read_file(workstation, args):
    answer = workstation.request(lambda invoke_id: services.atomic_read_file_request(
        invoke_id, setup.IVORY[1], 0, 1024), args.port)
    return services.parse_atomic_read_file_ack(answer), answer


def write_file(workstation, args, data):
    answer = workstation.request(lambda invoke_id: services.atomic_write_file_request(
        invoke_id, setup.IVORY[1], 0, data), args.port)
    return services.parse_atomic_write_file_ack(answer), answer


def last_restore_time(workstation, args):
    return setup.read_octets(workstation, args, setup.DEVICE, setup.PROPERTY_LAST_RESTORE_TIME)


def restore(workstation, args, image):
    """START_RESTORE, write `image`, END_RESTORE; returns the three answers."""
    started = reinitialize(workstation, args, wiretest.REINITIALIZE_START_RESTORE)
    wiretest.wait_until(lambda: system_status(workstation, args) != wiretest.DEVICE_STATUS_OPERATIONAL, STEP_WAIT)
    written, write_answer = write_file(workstation, args, image)
    return started, written, write_answer


def run(workstation, args):
    # 1. A configuration to back up.
    record("Setup: Analog Value 100 = 42.5 and 101 = 1.0 created",
           create(workstation, args, SETTING, 42.5) and create(workstation, args, SECOND, 1.0))

    # 2. Wrong password.
    answer = reinitialize(workstation, args, wiretest.REINITIALIZE_START_BACKUP, "not-the-password")
    record("FAILURE MODE: START_BACKUP with a wrong password -> SECURITY / PASSWORD_FAILURE",
           answer.error == (wiretest.ERROR_CLASS_SECURITY, wiretest.ERROR_CODE_PASSWORD_FAILURE), answer.describe())
    time.sleep(0.5)
    status = system_status(workstation, args)
    record("... and no backup starts: System_Status stays OPERATIONAL", status == wiretest.DEVICE_STATUS_OPERATIONAL,
           f"read {status}")

    # 3. Backup.
    answer = reinitialize(workstation, args, wiretest.REINITIALIZE_START_BACKUP)
    record("START_BACKUP with the password -> SimpleACK", answer.ok, answer.describe())
    in_progress = wiretest.wait_until(
        lambda: system_status(workstation, args) == wiretest.DEVICE_STATUS_BACKUP_IN_PROGRESS, STEP_WAIT)
    record("System_Status is BACKUP_IN_PROGRESS", in_progress, f"read {system_status(workstation, args)}")
    files = wiretest.decoded_values(setup.read_octets(workstation, args, setup.DEVICE, PROPERTY_CONFIGURATION_FILES))
    record("Configuration_Files names File 1", setup.IVORY in files, f"read {files}")
    contents, answer = read_file(workstation, args)
    image = contents[2] if contents else b""
    record("AtomicReadFile of File 1 returns the whole image (end-of-file TRUE)",
           contents is not None and contents[0] is True and image.startswith(b"B-AEC configuration 1\n"),
           f"{answer.describe() if contents is None else image!r}")
    record("The image holds Analog Value 100 at 42.5 and 101 at 1.0",
           b"analog-value 100 42.5\n" in image and b"analog-value 101 1\n" in image)
    answer = reinitialize(workstation, args, wiretest.REINITIALIZE_END_BACKUP)
    record("END_BACKUP -> SimpleACK", answer.ok, answer.describe())
    back = wiretest.wait_until(lambda: system_status(workstation, args) == wiretest.DEVICE_STATUS_OPERATIONAL, STEP_WAIT)
    record("System_Status is OPERATIONAL again", back, f"read {system_status(workstation, args)}")

    # 4. Change the configuration.
    changed = workstation.write_property(args.port, *SETTING, PROPERTY_PRESENT_VALUE, wiretest.application_real(7.0)).ok
    deleted = workstation.request(lambda invoke_id: services.delete_object_request(invoke_id, *SECOND), args.port).ok
    added = create(workstation, args, LATER, 3.0)
    record("The changed setting: 100 <- 7.0, 101 deleted, 102 created", changed and deleted and added)
    restored_before = last_restore_time(workstation, args)

    # 5. Restore. K.5.18: once a restore has started, a configuration file's File_Size is
    # writable when the image's size depends on the configuration (it does here), so a
    # client can empty the file before writing the backed-up image (135.1 13.8.1.1 step 21).
    started = reinitialize(workstation, args, wiretest.REINITIALIZE_START_RESTORE)
    record("START_RESTORE with the password -> SimpleACK", started.ok, started.describe())
    wiretest.wait_until(lambda: system_status(workstation, args) != wiretest.DEVICE_STATUS_OPERATIONAL, STEP_WAIT)
    answer = workstation.write_property(args.port, *setup.IVORY, PROPERTY_FILE_SIZE, wiretest.application_unsigned(0))
    record("WriteProperty File 1 File_Size = 0 during the restore -> SimpleACK", answer.ok, answer.describe())
    size = setup.read(workstation, args, setup.IVORY, PROPERTY_FILE_SIZE)
    record("... and File_Size reads 0", size == 0, f"read {size}")
    answer = workstation.write_property(args.port, *setup.IVORY, PROPERTY_FILE_SIZE,
                                        wiretest.application_unsigned(FILE_MAX_SIZE + 1))
    record("FAILURE MODE: File_Size larger than the file can hold -> PROPERTY / VALUE_OUT_OF_RANGE",
           answer.error == (wiretest.ERROR_CLASS_PROPERTY, wiretest.ERROR_CODE_VALUE_OUT_OF_RANGE), answer.describe())
    written, answer = write_file(workstation, args, image)
    size = setup.read(workstation, args, setup.IVORY, PROPERTY_FILE_SIZE)
    record("File_Size reads the image's size after AtomicWriteFile", size == len(image),
           f"read {size}, image {len(image)}")
    record("AtomicWriteFile of the backed-up image -> ACK at position 0", written == 0,
           f"ACK position {written}" if written is not None else answer.describe())
    value = present_value(workstation, args, SETTING)
    record("Control: before END_RESTORE, Analog Value 100 still reads 7.0", value == 7.0, f"read {value}")
    answer = reinitialize(workstation, args, wiretest.REINITIALIZE_END_RESTORE)
    record("END_RESTORE -> SimpleACK", answer.ok, answer.describe())

    # 6. Back to the backed-up configuration.
    restored = wiretest.wait_until(lambda: present_value(workstation, args, SETTING) == 42.5, STEP_WAIT)
    record("RESTORED: Analog Value 100 reads 42.5 again", restored, f"read {present_value(workstation, args, SETTING)}")
    value = present_value(workstation, args, SECOND)
    record("RESTORED: Analog Value 101 (deleted after the backup) exists again with 1.0", value == 1.0, f"read {value}")
    value = present_value(workstation, args, LATER)
    record("RESTORED: Analog Value 102 (created after the backup) is gone",
           value == (wiretest.ERROR_CLASS_OBJECT, wiretest.ERROR_CODE_UNKNOWN_OBJECT), f"read {value}")
    objects = setup.object_list(workstation, args) or []
    record("Object_List agrees: 100 and 101 listed, 102 not",
           SETTING in objects and SECOND in objects and LATER not in objects)
    restored_after = last_restore_time(workstation, args)
    record("Last_Restore_Time moved", restored_after is not None and restored_after != restored_before,
           f"{restored_before.hex() if restored_before else None} -> {restored_after.hex() if restored_after else None}")

    # 7. A file that is not a configuration image.
    # Last_Restore_Time has one-second resolution: wait past it, or a restore that
    # wrongly succeeded here could record the same second and pass the check below.
    time.sleep(1.1)
    # A change made since the restore, which a bad image must not undo.
    workstation.write_property(args.port, *SETTING, PROPERTY_PRESENT_VALUE, wiretest.application_real(9.0))
    started, written, answer = restore(workstation, args, b"not a configuration image\n")
    ended = reinitialize(workstation, args, wiretest.REINITIALIZE_END_RESTORE)
    record("FAILURE MODE: restore of a file that is not a configuration image (each step acknowledged)",
           started.ok and written == 0 and ended.ok,
           f"{started.describe()} / ACK position {written} / {ended.describe()}")
    wiretest.wait_until(lambda: system_status(workstation, args) == wiretest.DEVICE_STATUS_OPERATIONAL, STEP_WAIT)
    time.sleep(0.5)
    values = (present_value(workstation, args, SETTING), present_value(workstation, args, SECOND))
    record("... and the configuration is unchanged: 100 still reads 9.0 (written after the last restore), 101 1.0",
           values == (9.0, 1.0), f"read {values}")
    unchanged = last_restore_time(workstation, args)
    record("... and Last_Restore_Time does not move", unchanged == restored_after,
           f"{restored_after.hex() if restored_after else None} -> {unchanged.hex() if unchanged else None}")


def main():
    args = setup.parser(__doc__).parse_args()
    controller = setup.start_controller(args, ["--dcc-password", setup.PASSWORD])
    workstation = setup.workstation()
    try:
        run(workstation, args)
    finally:
        code = setup.finish(controller, workstation)
    return code


if __name__ == "__main__":
    sys.exit(main())
