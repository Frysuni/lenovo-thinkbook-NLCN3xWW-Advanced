/*
 * BiosAdvancedPatch.efi  (v4)
 *
 * Restores the hidden "Advanced" tab in the Lenovo SetupUtilityApp on
 * Insyde/Byosoft firmware that hides it, by neutralising one instruction in
 * the resident H2OFormBrowserDxe driver — in RAM, for the current boot only.
 * Flash is never touched; the change is gone after a reset.
 *
 * Why the tab disappears
 * ----------------------
 * H2OFormBrowserDxe's FormSet-sorting routine drops any FormSet whose internal
 * "show" flag byte (at [reg+0x10]) is 0:
 *
 *     cmp byte ptr [reg+0x10], 0     ; read the per-FormSet flag
 *     jz  <backward>                 ; flag==0 -> skip this FormSet
 *
 * The Advanced FormSet ships with flag=0, so it is removed from every
 * SendForm() call. Older firmware lacked this jump and showed Advanced.
 * NOP-ing the conditional jump (74/75 xx -> 90 90) lets every FormSet pass.
 *
 * How this app stays correct across versions/models
 * --------------------------------------------------
 *  1. It does NOT trust a fixed byte string. It finds the exact module that
 *     owns EFI_FORM_BROWSER2_PROTOCOL by taking that protocol's SendForm code
 *     pointer and locating the loaded image that contains it. That image *is*
 *     H2OFormBrowserDxe, whatever its name or layout.
 *  2. Inside (and only inside) that image it scans for the instruction idiom
 *     structurally (CMP byte [reg+0x10],0 ; J(N)Z backward), so register- and
 *     displacement-allocation changes between builds do not matter.
 *  3. Each site is patched with two write methods (memory-attribute remap and
 *     CR0.WP bypass) and then READ BACK, so success is never ambiguous.
 *
 * Use the companion analyze_formbrowser.py offline to confirm a new firmware
 * contains exactly this idiom before trusting the runtime patch on it.
 */

#include <efi.h>
#include <efilib.h>

/* EFI_FORM_BROWSER2_PROTOCOL — first member is SendForm (a code pointer). */
#define FORM_BROWSER2_GUID \
    { 0xb9d4c360, 0xbcfb, 0x4f9b, { 0x92,0x98,0x53,0xc1,0x36,0x98,0x22,0x58 } }
static EFI_GUID gFormBrowser2Guid = FORM_BROWSER2_GUID;

/* EFI_MEMORY_ATTRIBUTE_PROTOCOL (UEFI 2.10) — not in this gnu-efi. */
#define MEM_ATTR_PROTOCOL_GUID \
    { 0xf4560cf6, 0x40ec, 0x4b4a, { 0xa1,0x92,0xbf,0x1d,0x57,0xd0,0xb1,0x89 } }
#ifndef EFI_MEMORY_RO
#define EFI_MEMORY_RO  0x0000000000020000ULL
#endif
typedef struct _MEM_ATTR_PROTOCOL MEM_ATTR_PROTOCOL;
struct _MEM_ATTR_PROTOCOL {
    EFI_STATUS (EFIAPI *GetMemoryAttributes)(MEM_ATTR_PROTOCOL*, UINT64, UINT64, UINT64*);
    EFI_STATUS (EFIAPI *SetMemoryAttributes)(MEM_ATTR_PROTOCOL*, UINT64, UINT64, UINT64);
    EFI_STATUS (EFIAPI *ClearMemoryAttributes)(MEM_ATTR_PROTOCOL*, UINT64, UINT64, UINT64);
};
static EFI_GUID gMemAttrGuid = MEM_ATTR_PROTOCOL_GUID;

/*
 * Known-good exact anchor for the ThinkBook 14/16 G6+ AHP family
 * (NLCN37/38/39, byte-identical H2OFormBrowserDxe). Used as a guaranteed
 * fallback if the structural path matches nothing, so the proven case never
 * regresses. The J(N)Z to NOP is at offset +7..+8.
 *   ff 50 48  80 7b 10 00  74 de
 */
static const UINT8 KNOWN_ANCHOR[9] =
    { 0xFF, 0x50, 0x48, 0x80, 0x7B, 0x10, 0x00, 0x74, 0xDE };
#define KNOWN_JZ 7

/* ---- 64-bit hex formatter (no dependence on Print width specifiers) ---- */
static CHAR16 *Hex(UINT64 v, CHAR16 *buf /* >=17 */)
{
    const CHAR16 *d = L"0123456789ABCDEF";
    int i;
    for (i = 0; i < 16; i++) buf[15 - i] = d[(v >> (i * 4)) & 0xF];
    buf[16] = 0;
    return buf;
}

/* ---- CR0.WP control ---- */
static inline UINT64 ReadCr0(void)
{ UINT64 v; __asm__ __volatile__("mov %%cr0, %0" : "=r"(v)); return v; }
static inline void WriteCr0(UINT64 v)
{ __asm__ __volatile__("mov %0, %%cr0" :: "r"(v) : "memory"); }

/*
 * Match the loop-skip filter idiom at offset i:
 *   80 /7 [mod=01] disp8=0x10 imm8=0    (CMP byte [reg+0x10], 0)
 *   74|75 rel8(negative)                (J(N)Z backward)
 * Returns TRUE on match; the conditional-jump byte is at i+4.
 */
static BOOLEAN IsFilterSite(const UINT8 *p, UINTN i, UINTN n)
{
    UINT8 modrm, jop, jdisp;
    if (i + 6 > n)                 return FALSE;
    if (p[i] != 0x80)              return FALSE;
    modrm = p[i + 1];
    if ((modrm >> 6) != 1)         return FALSE;   /* mod = 01 (disp8)   */
    if (((modrm >> 3) & 7) != 7)   return FALSE;   /* /7 = CMP           */
    if ((modrm & 7) == 4)          return FALSE;   /* rm=100 needs SIB   */
    if (p[i + 2] != 0x10)          return FALSE;   /* disp8 == 0x10      */
    if (p[i + 3] != 0x00)          return FALSE;   /* imm8  == 0         */
    jop = p[i + 4];
    if (jop != 0x74 && jop != 0x75) return FALSE;  /* JZ / JNZ rel8      */
    jdisp = p[i + 5];
    if (jdisp < 0x80)              return FALSE;    /* backward only      */
    return TRUE;
}

/* Write two bytes to a possibly write-protected code page; verify by readback. */
static BOOLEAN ForceWrite2(UINT8 *dst, UINT8 b0, UINT8 b1)
{
    UINT64 cr0, page = (UINT64)dst & ~0xFFFULL;
    MEM_ATTR_PROTOCOL *ma = NULL;

    if (!EFI_ERROR(uefi_call_wrapper(BS->LocateProtocol, 3,
                                     &gMemAttrGuid, NULL, (VOID **)&ma)) && ma)
        uefi_call_wrapper(ma->ClearMemoryAttributes, 4,
                          ma, page, 0x2000ULL, EFI_MEMORY_RO);

    __asm__ __volatile__("cli");
    cr0 = ReadCr0();
    WriteCr0(cr0 & ~(1ULL << 16));
    dst[0] = b0; dst[1] = b1;
    __asm__ __volatile__("" ::: "memory");
    WriteCr0(cr0);
    __asm__ __volatile__("sti");

    return dst[0] == b0 && dst[1] == b1;
}

/* Find the loaded image whose VA range contains 'addr'. */
static EFI_LOADED_IMAGE *
ImageContaining(EFI_HANDLE *Handles, UINTN Count, UINT64 addr)
{
    UINTN i;
    for (i = 0; i < Count; i++) {
        EFI_LOADED_IMAGE *Li = NULL;
        if (EFI_ERROR(uefi_call_wrapper(BS->HandleProtocol, 3,
                Handles[i], &LoadedImageProtocol, (VOID **)&Li)))
            continue;
        if (!Li || !Li->ImageBase || !Li->ImageSize) continue;
        if (addr >= (UINT64)Li->ImageBase &&
            addr <  (UINT64)Li->ImageBase + (UINT64)Li->ImageSize)
            return Li;
    }
    return NULL;
}

EFI_STATUS EFIAPI
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    EFI_STATUS        Status;
    EFI_HANDLE       *Handles = NULL;
    UINTN             Count = 0, i, idx;
    VOID             *Fb2 = NULL;
    UINT64            SendForm;
    EFI_LOADED_IMAGE *Li;
    UINT8            *Base;
    UINTN             Size, patched = 0, sites = 0;
    CHAR16            hb[17];

    InitializeLib(ImageHandle, SystemTable);
    Print(L"\r\n=== BiosAdvancedPatch v4 ===\r\n");
    Print(L"Locating the form browser and disabling the Advanced-tab filter.\r\n\r\n");

    /* Handle buffer is needed by both the structural and the fallback paths. */
    Status = uefi_call_wrapper(BS->LocateHandleBuffer, 5,
                               ByProtocol, &LoadedImageProtocol,
                               NULL, &Count, &Handles);
    if (EFI_ERROR(Status)) {
        Print(L"[FAIL] LocateHandleBuffer: %r\r\n", Status);
        goto WaitKey;
    }

    /* Primary path: locate H2OFormBrowserDxe via the protocol it owns, then
     * structurally scan only that image. Non-fatal — falls through to the
     * exact-anchor sweep if anything here comes up empty. */
    Status = uefi_call_wrapper(BS->LocateProtocol, 3,
                               &gFormBrowser2Guid, NULL, &Fb2);
    if (!EFI_ERROR(Status) && Fb2) {
        SendForm = *(UINT64 *)Fb2;          /* first member = SendForm() */
        Print(L"FormBrowser2 at 0x%s, SendForm=0x%s\r\n",
              Hex((UINT64)Fb2, hb), Hex(SendForm, hb));
        Li = ImageContaining(Handles, Count, SendForm);
        if (Li) {
            Base = (UINT8 *)Li->ImageBase;
            Size = (UINTN)Li->ImageSize;
            Print(L"Form browser image: base=0x%s size=0x%s\r\n\r\n",
                  Hex((UINT64)Base, hb), Hex((UINT64)Size, hb));
            for (i = 0; i + 6 <= Size; i++) {
                if (!IsFilterSite(Base, i, Size)) continue;
                sites++;
                {
                    UINT8 *jz = Base + i + 4;   /* the J(N)Z opcode+disp */
                    UINT8 b0 = jz[0], b1 = jz[1];
                    BOOLEAN ok;
                    Print(L"[site] +0x%s : cmp [reg+10],0 ; %02x %02x (jcc)\r\n",
                          Hex((UINT64)i, hb), b0, b1);
                    ok = ForceWrite2(jz, 0x90, 0x90);
                    Print(L"       after: %02x %02x  %s\r\n", jz[0], jz[1],
                          ok ? L"-> OK (verified)" : L"-> FAILED (write-protected)");
                    if (ok) patched++;
                }
            }
        } else {
            Print(L"SendForm not inside any loaded image; using fallback.\r\n");
        }
    } else {
        Print(L"FormBrowser2 not found (%r); using fallback.\r\n", Status);
    }

    Print(L"\r\nStructural sites: %d   patched OK: %d\r\n", (int)sites, (int)patched);

    /*
     * Fallback: if the structural scan matched nothing in the located image
     * (unexpected layout, or protocol owned by a wrapper), sweep every loaded
     * image for the exact known G6+ AHP anchor. This is the proven v3 path.
     */
    if (patched == 0) {
        Print(L"Trying exact known anchor (G6+ AHP family)...\r\n");
        for (i = 0; i < Count; i++) {
            EFI_LOADED_IMAGE *Fi = NULL;
            UINT8 *B; UINTN S, o;
            if (EFI_ERROR(uefi_call_wrapper(BS->HandleProtocol, 3,
                    Handles[i], &LoadedImageProtocol, (VOID **)&Fi)))
                continue;
            if (!Fi || !Fi->ImageBase || !Fi->ImageSize) continue;
            B = (UINT8 *)Fi->ImageBase; S = (UINTN)Fi->ImageSize;
            for (o = 0; o + sizeof(KNOWN_ANCHOR) <= S; o++) {
                UINTN k; BOOLEAN hit = TRUE;
                for (k = 0; k < sizeof(KNOWN_ANCHOR); k++)
                    if (B[o + k] != KNOWN_ANCHOR[k]) { hit = FALSE; break; }
                if (!hit) continue;
                sites++;
                {
                    UINT8 *jz = B + o + KNOWN_JZ;
                    BOOLEAN ok = ForceWrite2(jz, 0x90, 0x90);
                    Print(L"[anchor] image base=0x%s +0x%s  %s\r\n",
                          Hex((UINT64)B, hb), Hex((UINT64)o, hb),
                          ok ? L"-> OK (verified)" : L"-> FAILED");
                    if (ok) patched++;
                }
            }
        }
    }

    if (sites == 0) {
        Print(L"\r\n[INFO] No filter present in this build.\r\n");
        Print(L"       Advanced is already reachable; nothing to do.\r\n");
        Status = EFI_SUCCESS;
    } else if (patched == 0) {
        Print(L"\r\n[FAIL] Found the filter but the write was blocked.\r\n");
        Status = EFI_ACCESS_DENIED;
    } else {
        Print(L"\r\n[DONE] Advanced filter disabled for this boot.\r\n");
        Print(L"       Now run SetupUtilityApp; the Advanced tab will appear.\r\n");
        Status = EFI_SUCCESS;
    }

    uefi_call_wrapper(BS->FreePool, 1, Handles);
WaitKey:
    Print(L"\r\nPress any key to exit...\r\n");
    uefi_call_wrapper(BS->WaitForEvent, 3, 1, &ST->ConIn->WaitForKey, &idx);
    return Status;
}
