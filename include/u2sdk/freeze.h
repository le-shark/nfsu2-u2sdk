#pragma once
// World freeze through the game's own pause branch, shared by every ASI built on u2sdk.
//
// GameLoop decides at 0x58132C whether the game is paused (verified-in-binary.md "World freeze"):
//   0x58132C  movzx eax, al      ; al = paused (RaceCoordinator state 6 = pause menu)
//   0x58132F  cmp eax, ebx       ; ebx = 0
//   0x581331  je  0x581349       ; not paused -> sim block, World::DoTimestep 0x609CE0(scaledDt)
//   paused:   0x601A60(dt=0), store sim dt, jmp 0x581465 (skips the whole simulation block)
// Those 5 bytes are redirected to a stub that reports "paused" while any holder wants the world frozen, so the
// game takes its own pause path; camera movers and rendering keep running. (Tweak_GameSpeed = 0 is NOT
// equivalent: it still runs DoTimestep with dt = 0 and left the game frozen for good, [game] 2026-10-02.)
//
// Several ASIs (photo mode, the daylight tuner) need this site. The first one to call Install patches it; the
// holders live in a small named shared block (per process), so every ASI's Hold reaches the one stub.
namespace u2::freeze {

bool Install();         // false if the bytes at 0x58132C are neither the game's nor our stub (another mod)
void Hold(bool on);     // this ASI's hold; the world is frozen while any ASI holds it
bool Held();            // this ASI holds it
bool AnyHeld();         // any ASI holds it

}  // namespace u2::freeze
