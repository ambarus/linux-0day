=================
SPI NOR framework
=================

Part I - Why do we need this framework?
---------------------------------------

SPI bus controllers (drivers/spi/) only deal with streams of bytes; the bus
controller operates agnostic of the specific device attached. However, some
controllers (such as Freescale's QuadSPI controller) cannot easily handle
arbitrary streams of bytes, but rather are designed specifically for SPI NOR.

In particular, Freescale's QuadSPI controller must know the NOR commands to
find the right LUT sequence. Unfortunately, the SPI subsystem has no notion of
opcodes, addresses, or data payloads; a SPI controller simply knows to send or
receive bytes (Tx and Rx). Therefore, we must define a new layering scheme under
which the controller driver is aware of the opcodes, addressing, and other
details of the SPI NOR protocol.

Part II - How does the framework work?
--------------------------------------

This framework just adds a new layer between the MTD and the SPI bus driver.
With this new layer, the SPI NOR controller driver does not depend on the
m25p80 code anymore.

Before this framework, the layer is like::

                   MTD
         ------------------------
                  m25p80
         ------------------------
	       SPI bus driver
         ------------------------
	        SPI NOR chip

After this framework, the layer is like::

                   MTD
         ------------------------
              SPI NOR framework
         ------------------------
                  m25p80
         ------------------------
	       SPI bus driver
         ------------------------
	       SPI NOR chip

With the SPI NOR controller driver (Freescale QuadSPI), it looks like::

                   MTD
         ------------------------
              SPI NOR framework
         ------------------------
                fsl-quadSPI
         ------------------------
	       SPI NOR chip

Part III - How can drivers use the framework?
---------------------------------------------

The main API is spi_nor_scan(). Before you call the hook, a driver should
initialize the necessary fields for spi_nor{}. Please see
drivers/mtd/spi-nor/spi-nor.c for detail. Please also refer to spi-fsl-qspi.c
when you want to write a new driver for a SPI NOR controller.
Another API is spi_nor_restore(), this is used to restore the status of SPI
flash chip such as addressing mode. Call it whenever detach the driver from
device or reboot the system.

Part IV - How to propose a new flash addition?
----------------------------------------------

First we have to clarify where the new flash_info entry will reside. Typically
each manufacturer have their own driver and the new flash will be placed in that
specific manufacturer driver. There are cases however, where special care has to
be taken. In case of flash ID collisions between different manufacturers, the
place to add the new flash is in the manuf-id-collisions.c driver. ID collisions
between flashes of the same manufacturer should be handled in their own
manufacturer driver, macronix being an example. There will be a single
flash_info entry for all the ID collisions of the same ID.

manuf-id-collisions.c is the place to add new flash additions where the
manufacturer is ignorant enough to not implement the ID continuation scheme
that is described in the JEP106 JEDEC Standard. One has to dump its flash ID and
compare it with the flash's manufacturer identification code that is defined in
the JEP106 JEDEC Standard. If the manufacturer ID is defined in bank two or
higher and the manufacturer does not implement the ID continuation scheme, then
it is likely that the flash ID will collide with a manufacturer from bank one or
with other manufacturer from other bank that does not implement the ID
continuation scheme as well.

flash_info entries will be added in a first come, first served manner. If there
are ID collisions, differentiation between flashes will be done at runtime if
possible. Where runtime differentiation is not possible, new compatibles will be
introduced, but this will be done as a last resort.

New flash additions that support the SFDP standard should be declared using
SPI_NOR_PARSE_SFDP. Support that can be discovered when parsing SFDP should not
be duplicated by explicit flags at flash declaration. All the SFDP flash
parameters and settings will be discovered when parsing SFDP. There are
flash_info flags that indicate support that is not SFDP discoverable. These
flags initialize non SFDP support in the spi_nor_nonsfdp_flags_init() method.
SPI_NOR_PARSE_SFDP is usually followed by other flash_info flags from the
aforementioned function. Sometimes manufacturers wrongly define some fields in
the SFDP tables. If that's the case, SFDP data can be amended with the fixups()
hooks. It is not common, but if the SFDP tables are entirely wrong, and it does
not worth the hassle to tweak the SFDP parameters by using the fixups hooks, or
if the flash does not define the SFDP tables at all, then one can statically
init the flash with the SPI_NOR_SKIP_SFDP flag and specify the rest of the flash
capabilities with the flash info flags.

With time we want to convert all flashes to either use SPI_NOR_PARSE_SFDP or
SPI_NOR_SKIP_SFDP and stop triggering the SFDP parsing with the
SPI_NOR_{DUAL, QUAD, OCTAL*}_READ flags. There are flashes that support QUAD
mode but do not support the RDSFDP command, we should avoid issuing unsupported
commands to flashes where possible. It is unlikely that RDSFDP will cause any
problems, but still, it's better to avoid it. There are cases however of flash
ID collisions between flashes that define the SFDP tables and flashes that don't
(again, macronix). We usually differentiate between the two by issuing the
RDSFDP command. In such a case one has to declare the SPI_NOR_PARSE_SFDP
together with all the relevant flags from spi_nor_nonsfdp_flags_init() for the
SFDP compatible flash, but should also declare the relevant flags that are used
in the spi_nor_info_init_params() method in order to init support that can't be
discovered via SFDP for the non-SFDP compatible flash.

Every new flash addition that define the SFDP tables, should hexdump its SFDP
tables in the patch's comment section below the --- line, so that we can
reference it in case of ID collisions.

Every flash_info flag declared should be tested. Typically one uses the
mtd-utils and does an erase, verify erase, write, read back and compare test.
Locking and other flags that are declared in the flash_info entry and used in
the spi_nor_nonsfdp_flags_init() should be tested as well.
