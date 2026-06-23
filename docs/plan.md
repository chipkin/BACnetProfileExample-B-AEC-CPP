# Plan (STUB): B-AEC (Advanced Elevator Controller) — C++ example

> **STATUS: STUB.** Seed facts below. Expand from
> [`bacnet-profile-plan-template.md`](../../bacnet-profile-plan-template.md) after the
> sample plans ([B-LD](../../BACnetProfileExample-B-LD-CPP/docs/plan.md),
> [B-BC](../../BACnetProfileExample-B-BC-CPP/docs/plan.md)) are reviewed.

**Profile:** B-AEC · **Family:** Annex L.13 (Elevator Controller) · **Role:** B ·
**Archetype:** Controller · **Difficulty:** 5/5 · **Build wave:** 4 (builds on B-EC)

**Thesis:** B-EC **plus** Event Log, scheduling, **object create/delete**, and
backup/restore. Canonical source for **F-OCD**.

## Required BIBBs (profiles.md L.13)
B-EC's set **+ AE-EL-I-B; SCHED-I-B; DM-OCD-B, DM-BR-B**.

## Services to enable
- All of B-EC's services + Event Log + Schedule (read-only) + CreateObject/
  DeleteObject (DM-OCD-B) + backup/restore.

## Objects (baseline + B-EC objects + )
- Event Log 1, Schedule 1 + Calendar 1 (read-only), File 1 (backup).

## Shared features
- **DEFINE:** F-OCD (Object Create/Delete — CreateObject/DeleteObject callbacks).
- **REUSE:** everything from B-EC + F-EVENTLOG (B-ALSC), F-SCHED (read-only),
  F-BACKUP (B-ACC).

## Known stack gaps
- Schedule engine (F-SCHED read-only), backup/restore reality (F-BACKUP),
  recipient-by-address (F-ALARM). profiles.md: ✅ S69 (DM-BR-B via 108 File + 12
  Backup/Restore gtests).

## Notes / open questions
- Build **after B-EC, B-ALSC (F-EVENTLOG), and B-ACC (F-BACKUP)**. F-OCD is the only
  new feature — confirm the CreateObject/DeleteObject callback API in the DLL.
