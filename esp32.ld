/* Linker script for the ESP8266 */

MEMORY
{
    /* Data RAM. Allows byte access. */
    DRAM  (rw) : ORIGIN = 0x3FFAE400, LENGTH = 199K + 128K /* Internal SRAM 1 + 2 */
    /* Instruction RAM. */
    IRAM  (x)  : ORIGIN = 0x40080000, LENGTH = 128K /* Internal SRAM 0 */
}

/* The entry point. It is set in the image flashed to the chip, so must be
 * defined.
 */
ENTRY(main)

SECTIONS
{
    /* Mutable global variables.
     */
    .data : ALIGN(4)
    {
        *(.data)
        *(.data.*)
    } >DRAM

    /* Constant global variables.
     * Note that they still need to be loaded in RAM because the ESP8266 doesn't
     * allow byte access to flash.
     */
    .rodata : ALIGN(4)
    {
        *(.rodata)
        *(.rodata.*)
    } >DRAM

    /* Global variables that are mutable and zero-initialized.
     */
    .bss (NOLOAD) : ALIGN(4)
    {
        . = ALIGN (4);
        _sbss = ABSOLUTE(.);
        *(.bss)
        *(.bss.*)
        . = ALIGN (4);
        _ebss = ABSOLUTE(.);
    } >DRAM

    /* Constant literals and code. Loaded into IRAM for now. Eventually, most
     * code should be executed directly from flash.
     * Note that literals must be before code for the l32r instruction to work.
     */
    .text : ALIGN(4)
    {
        *(.literal .text)
        *(.literal.* .text.*)
    } >IRAM
}

/* libgcc functions available in the ROM. Use them whenever possible.
 */
__absvdi2      = 0x4006387c;
__absvsi2      = 0x40063868;
__adddf3       = 0x40002590;
__addsf3       = 0x400020e8;
__addvdi3      = 0x40002cbc;
__addvsi3      = 0x40002c98;
__ashldi3      = 0x4000c818;
__ashrdi3      = 0x4000c830;
__bswapdi2     = 0x40064b08;
__bswapsi2     = 0x40064ae0;
__clrsbdi2     = 0x40064b7c;
__clrsbsi2     = 0x40064b64;
__clzdi2       = 0x4000ca50;
__clzsi2       = 0x4000c7e8;
__cmpdi2       = 0x40063820;
__ctzdi2       = 0x4000ca64;
__ctzsi2       = 0x4000c7f0;
__divdc3       = 0x400645a4;
__divdf3       = 0x40002954;
__divdi3       = 0x4000ca84;
__divsi3       = 0x4000c7b8;
__eqdf2        = 0x400636a8;
__eqsf2        = 0x40063374;
__extendsfdf2  = 0x40002c34;
__ffsdi2       = 0x4000ca2c;
__ffssi2       = 0x4000c804;
__fixdfdi      = 0x40002ac4;
__fixdfsi      = 0x40002a78;
__fixsfdi      = 0x4000244c;
__fixsfsi      = 0x4000240c;
__fixunsdfsi   = 0x40002b30;
__fixunssfdi   = 0x40002504;
__fixunssfsi   = 0x400024ac;
__floatdidf    = 0x4000c988;
__floatdisf    = 0x4000c8c0;
__floatsidf    = 0x4000c944;
__floatsisf    = 0x4000c870;
__floatundidf  = 0x4000c978;
__floatundisf  = 0x4000c8b0;
__floatunsidf  = 0x4000c938;
__floatunsisf  = 0x4000c864;
__gcc_bcmp     = 0x40064a70;
__gedf2        = 0x40063768;
__gesf2        = 0x4006340c;
__gtdf2        = 0x400636dc;
__gtsf2        = 0x400633a0;
__ledf2        = 0x40063704;
__lesf2        = 0x400633c0;
__lshrdi3      = 0x4000c84c;
__ltdf2        = 0x40063790;
__ltsf2        = 0x4006342c;
__moddi3       = 0x4000cd4c;
__modsi3       = 0x4000c7c0;
__muldc3       = 0x40063c90;
__muldf3       = 0x4006358c;
__muldi3       = 0x4000c9fc;
__mulsf3       = 0x400632c8;
__mulsi3       = 0x4000c7b0;
__mulvdi3      = 0x40002d78;
__mulvsi3      = 0x40002d60;
__nedf2        = 0x400636a8;
__negdf2       = 0x400634a0;
__negdi2       = 0x4000ca14;
__negsf2       = 0x400020c0;
__negvdi2      = 0x40002e98;
__negvsi2      = 0x40002e78;
__nesf2        = 0x40063374;
__nsau_data    = 0x3ff96544;
__paritysi2    = 0x40002f3c;
__popcount_tab = 0x3ff96544;
__popcountdi2  = 0x40002ef8;
__popcountsi2  = 0x40002ed0;
__powidf2      = 0x400638e4;
__subdf3       = 0x400026e4;
__subsf3       = 0x400021d0;
__subvdi3      = 0x40002d20;
__subvsi3      = 0x40002cf8;
__truncdfsf2   = 0x40002b90;
__ucmpdi2      = 0x40063840;
__udiv_w_sdiv  = 0x40064bec;
__udivdi3      = 0x4000cff8;
__udivmoddi4   = 0x40064bf4;
__udivsi3      = 0x4000c7c8;
__umoddi3      = 0x4000d280;
__umodsi3      = 0x4000c7d0;
__umulsidi3    = 0x4000c7d8;
__unorddf2     = 0x400637f4;
__unordsf2     = 0x40063478;

ets_isr_unmask = 0x40006808;
_xtos_set_interrupt_handler = 0x4000bf78;
_xtos_set_interrupt_handler_arg = 0x4000bf34;

abs = 0x40056340;
__ascii_wctomb = 0x40058ef0;
atoi = 0x400566c4;
_atoi_r = 0x400566d4;
atol = 0x400566ec;
_atol_r = 0x400566fc;
bzero = 0x4000c1f4;
_cleanup = 0x40001df8;
_cleanup_r = 0x40001d48;
creat = 0x40000e8c;
div = 0x40056348;
__dummy_lock = 0x4000c728;
__dummy_lock_try = 0x4000c730;
__env_lock = 0x40001fd4;
__env_unlock = 0x40001fe0;
fclose = 0x400020ac;
_fclose_r = 0x40001fec;
fflush = 0x40059394;
_fflush_r = 0x40059320;
_findenv_r = 0x40001f44;
__fp_lock_all = 0x40001f1c;
__fp_unlock_all = 0x40001f30;
__fputwc = 0x40058da0;
fputwc = 0x40058ea8;
_fputwc_r = 0x40058e4c;
_fwalk = 0x4000c738;
_fwalk_reent = 0x4000c770;
_getenv_r = 0x40001fbc;
isalnum = 0x40000f04;
isalpha = 0x40000f18;
isascii = 0x4000c20c;
_isatty_r = 0x40000ea0;
isblank = 0x40000f2c;
iscntrl = 0x40000f50;
isdigit = 0x40000f64;
isgraph = 0x40000f94;
islower = 0x40000f78;
isprint = 0x40000fa8;
ispunct = 0x40000fc0;
isspace = 0x40000fd4;
isupper = 0x40000fe8;
__itoa = 0x40056678;
itoa = 0x400566b4;
labs = 0x40056370;
ldiv = 0x40056378;
longjmp = 0x400562cc;
memccpy = 0x4000c220;
memchr = 0x4000c244;
memcmp = 0x4000c260;
memcpy = 0x4000c2c8;
memmove = 0x4000c3c0;
memrchr = 0x4000c400;
memset = 0x4000c44c;
qsort = 0x40056424;
rand = 0x40001058;
rand_r = 0x400010d4;
__sccl = 0x4000c498;
__sclose = 0x400011b8;
__seofread = 0x40001148;
setjmp = 0x40056268;
__sflush_r = 0x400591e0;
__sfmoreglue = 0x40001dc8;
__sfp = 0x40001e90;
__sfp_lock_acquire = 0x40001e08;
__sfp_lock_release = 0x40001e14;
__sfvwrite_r = 0x4005893c;
__sinit = 0x40001e38;
__sinit_lock_acquire = 0x40001e20;
__sinit_lock_release = 0x40001e2c;
__smakebuf_r = 0x40059108;
srand = 0x40001004;
__sread = 0x40001118;
__srefill_r = 0x400593d4;
__sseek = 0x40001184;
strcasecmp = 0x400011cc;
strcasestr = 0x40001210;
strcat = 0x4000c518;
strchr = 0x4000c53c;
strcmp = 0x40001274;
strcoll = 0x40001398;
strcpy = 0x400013ac;
strcspn = 0x4000c558;
strdup = 0x4000143c;
_strdup_r = 0x40001450;
strlcat = 0x40001470;
strlcpy = 0x4000c584;
strlen = 0x400014c0;
strlwr = 0x40001524;
strncasecmp = 0x40001550;
strncat = 0x4000c5c4;
strncmp = 0x4000c5f4;
strncpy = 0x400015d4;
strndup = 0x400016b0;
_strndup_r = 0x400016c4;
strnlen = 0x4000c628;
strrchr = 0x40001708;
strsep = 0x40001734;
strspn = 0x4000c648;
strstr = 0x4000c674;
__strtok_r = 0x4000c6a8;
strtok_r = 0x4000c70c;
strtol = 0x4005681c;
_strtol_r = 0x40056714;
strtoul = 0x4005692c;
_strtoul_r = 0x40056834;
strupr = 0x4000174c;
__submore = 0x40058f3c;
__swbuf = 0x40058cb4;
__swbuf_r = 0x40058bec;
__swrite = 0x40001150;
__swsetup_r = 0x40058cc8;
toascii = 0x4000c720;
tolower = 0x40001868;
toupper = 0x40001884;
ungetc = 0x400590f4;
_ungetc_r = 0x40058fa0;
__utoa = 0x400561f0;
utoa = 0x40056258;
wcrtomb = 0x40058920;
_wcrtomb_r = 0x400588d8;
_wctomb_r = 0x40058f14;

PROVIDE ( _ctype_ = 0x3ff96354 );
PROVIDE ( __ctype_ptr__ = 0x3ff96350 );

PROVIDE ( asiprintf = 0x40056d9c );
PROVIDE ( _asiprintf_r = 0x40056d4c );
PROVIDE ( asniprintf = 0x40056cd8 );
PROVIDE ( _asniprintf_r = 0x40056c64 );
PROVIDE ( asnprintf = 0x40056cd8 );
PROVIDE ( _asnprintf_r = 0x40056c64 );
PROVIDE ( asprintf = 0x40056d9c );
PROVIDE ( _asprintf_r = 0x40056d4c );
PROVIDE ( fiprintf = 0x40056efc );
PROVIDE ( _fiprintf_r = 0x40056ed8 );
PROVIDE ( fiscanf = 0x40058884 );
PROVIDE ( _fiscanf_r = 0x400588b4 );
PROVIDE ( fprintf = 0x40056efc );
PROVIDE ( _fprintf_r = 0x40056ed8 );
PROVIDE ( iprintf = 0x40056978 );
PROVIDE ( _iprintf_r = 0x40056944 );
PROVIDE ( printf = 0x40056978 );
PROVIDE ( _printf_common = 0x40057338 );
PROVIDE ( _printf_float = 0x4000befc );
PROVIDE ( _printf_i = 0x40057404 );
PROVIDE ( _printf_r = 0x40056944 );
PROVIDE ( siprintf = 0x40056c08 );
PROVIDE ( _siprintf_r = 0x40056bbc );
PROVIDE ( sniprintf = 0x40056b4c );
PROVIDE ( _sniprintf_r = 0x40056ae4 );
PROVIDE ( snprintf = 0x40056b4c );
PROVIDE ( _snprintf_r = 0x40056ae4 );
PROVIDE ( sprintf = 0x40056c08 );
PROVIDE ( _sprintf_r = 0x40056bbc );
PROVIDE ( __sprint_r = 0x400577e4 );
PROVIDE ( _svfiprintf_r = 0x40057100 );
PROVIDE ( __svfiscanf_r = 0x40057b08 );
PROVIDE ( _svfprintf_r = 0x40057100 );
PROVIDE ( __svfscanf = 0x40057f04 );
PROVIDE ( __svfscanf_r = 0x40057b08 );
PROVIDE ( vasiprintf = 0x40056eb8 );
PROVIDE ( _vasiprintf_r = 0x40056e80 );
PROVIDE ( vasniprintf = 0x40056e58 );
PROVIDE ( _vasniprintf_r = 0x40056df8 );
PROVIDE ( vasnprintf = 0x40056e58 );
PROVIDE ( _vasnprintf_r = 0x40056df8 );
PROVIDE ( vasprintf = 0x40056eb8 );
PROVIDE ( _vasprintf_r = 0x40056e80 );
PROVIDE ( vfiprintf = 0x40057ae8 );
PROVIDE ( _vfiprintf_r = 0x40057850 );
PROVIDE ( vfiscanf = 0x40057eb8 );
PROVIDE ( _vfiscanf_r = 0x40057f24 );
PROVIDE ( vfprintf = 0x40057ae8 );
PROVIDE ( _vfprintf_r = 0x40057850 );
PROVIDE ( vfscanf = 0x40057eb8 );
PROVIDE ( _vfscanf_r = 0x40057f24 );
PROVIDE ( viprintf = 0x400569b4 );
PROVIDE ( _viprintf_r = 0x400569e4 );
PROVIDE ( viscanf = 0x40058698 );
PROVIDE ( _viscanf_r = 0x400586c8 );
PROVIDE ( vprintf = 0x400569b4 );
PROVIDE ( _vprintf_r = 0x400569e4 );
PROVIDE ( vscanf = 0x40058698 );
PROVIDE ( _vscanf_r = 0x400586c8 );
PROVIDE ( vsiprintf = 0x40056ac4 );
PROVIDE ( _vsiprintf_r = 0x40056a90 );
PROVIDE ( vsiscanf = 0x40058740 );
PROVIDE ( _vsiscanf_r = 0x400586f8 );
PROVIDE ( vsniprintf = 0x40056a68 );
PROVIDE ( _vsniprintf_r = 0x40056a14 );
PROVIDE ( vsnprintf = 0x40056a68 );
PROVIDE ( _vsnprintf_r = 0x40056a14 );
PROVIDE ( vsprintf = 0x40056ac4 );
PROVIDE ( _vsprintf_r = 0x40056a90 );
PROVIDE ( vsscanf = 0x40058740 );
PROVIDE ( _vsscanf_r = 0x400586f8 );
PROVIDE ( fscanf = 0x40058884 );
PROVIDE ( _fscanf_r = 0x400588b4 );
PROVIDE ( iscanf = 0x40058760 );
PROVIDE ( _iscanf_r = 0x4005879c );
PROVIDE ( scanf = 0x40058760 );
PROVIDE ( _scanf_chars = 0x40058384 );
PROVIDE ( _scanf_float = 0x4000bf18 );
PROVIDE ( _scanf_i = 0x4005845c );
PROVIDE ( _scanf_r = 0x4005879c );
PROVIDE ( siscanf = 0x400587d0 );
PROVIDE ( _siscanf_r = 0x40058830 );
PROVIDE ( sscanf = 0x400587d0 );
PROVIDE ( _sscanf_r = 0x40058830 );
PROVIDE ( __ssvfiscanf_r = 0x4005802c );
PROVIDE ( __ssvfscanf_r = 0x4005802c );

/* These are still part of that driver, but have been verified not to use static RAM, so they can be used. */
PROVIDE ( gpio_output_set = 0x40009b24 );
PROVIDE ( gpio_output_set_high = 0x40009b5c );
PROVIDE ( gpio_input_get = 0x40009b88 );
PROVIDE ( gpio_input_get_high = 0x40009b9c );
PROVIDE ( gpio_matrix_in = 0x40009edc );
PROVIDE ( gpio_matrix_out = 0x40009f0c );
PROVIDE ( gpio_pad_select_gpio = 0x40009fdc );
PROVIDE ( gpio_pad_set_drv = 0x4000a11c );
PROVIDE ( gpio_pad_pulldown = 0x4000a348 );
PROVIDE ( gpio_pad_pullup = 0x4000a22c );
PROVIDE ( gpio_pad_hold = 0x4000a734 );
PROVIDE ( gpio_pad_unhold = 0x4000a484 );

PROVIDE ( gsGPIO = 0x3ff44000 );
PROVIDE ( grIOMUX = 0x3ff49000 );
PROVIDE ( gsDPORT = 0x3ff00000);
PROVIDE ( gsPIDCTRL = 0x3ff1f000);
PROVIDE ( gsRTC = 0x3ff48000);
PROVIDE ( gsTIMG0 = 0x3ff5f000);
PROVIDE ( gsTIMG1 = 0x3ff60000);
PROVIDE ( gsI2C0 = 0x3ff53000);
PROVIDE ( gsI2C1 = 0x3ff67000);
PROVIDE ( gsUART0 = 0x60000000);
PROVIDE ( gsUART1 = 0x60010000);
PROVIDE ( gsUART2 = 0x602E0000);
PROVIDE ( gsUART0Mapped = 0x3ff40000);
PROVIDE ( gsUART1Mapped = 0x3ff50000);
PROVIDE ( gsUART2Mapped = 0x3ff6e000);
