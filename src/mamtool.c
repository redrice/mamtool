#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>

#include <gc/gc.h>
#include <utlist.h>

#include "uscsilib.h"

#define CDB_OPCODE		0
#define CDB_RDATTR_SVCACTION	1 
#define CDB_RDATTR_ID_MSB	8
#define CDB_RDATTR_ID_LSB	9
#define CDB_RDATTR_ALLOCLEN_LSB	13

#define CDB_RDATTR_SVCACTION_AVAL	0x0	/* attribute values */
#define CDB_RDATTR_SVCACTION_ALIST	0x1	/* attribute list */
#define CDB_RDATTR_SVCACTION_LVLIST	0x2	/* logical volume list */
#define CDB_RDATTR_SVCACTION_PRLIST	0x3	/* partition list */
/* my LTO-4 drive does not support this operation */
#define CDB_RDATTR_SVCACTION_SALIST	0x5	/* supported attributes list */

#define ATTR_LIST_AVAILABLE CDB_RDATTR_SVCACTION_ALIST
#define ATTR_LIST_SUPPORTED CDB_RDATTR_SVCACTION_SALIST

#define OP_READ_ATTRIBUTE	0x8C
#define OP_WRITE_ATTRIBUTE	0x8D

#define RDATTR_LISTHEAD_LEN	4	/* attribute list has 4 byte header */
#define RDATTR_ATTRHEAD_LEN	5	/* every attribute has 5 byte header */
#define RDATTR_HEADONLY_LEN	RDATTR_LISTHEAD_LEN+RDATTR_ATTRHEAD_LEN	

#define RDATTR_ATTRHEAD_ID_MSB		0
#define RDATTR_ATTRHEAD_ID_LSB		1
#define RDATTR_ATTRHEAD_FORMAT		2
#define RDATTR_ATTRHEAD_ATTRLEN_MSB	3
#define RDATTR_ATTRHEAD_ATTRLEN_LSB	4

#define ATTR_FORMAT_MASK		0x03
#define ATTR_RO_MASK			0x80

#define ATTR_FORMAT_BINARY		0
#define ATTR_FORMAT_ASCII		1
#define ATTR_FORMAT_TEXT		2

struct mam_attribute {
	uint16_t id;
	uint16_t length;
	uint8_t format;
	bool ro;
	uint8_t *value;
};

struct mam_id_list {
	uint16_t id;

	struct mam_id_list *next;
};

struct uscsi_dev dev;

static const char *default_tape = "/dev/enrst0";

bool flag_verbose = true; /* XXX */

static char const *
attribute_format_to_string(uint8_t format)
{
	switch (format) {
	case 0x0: return "binary";
	case 0x1: return "ascii";
	case 0x2: return "text";
	}

	return "unknown/reserved";

}

static char const *
attribute_ro_to_string(bool ro)
{
	if (ro)
		return "read-only";

	return "read-write";
}

static char const *
attribute_id_to_string(uint16_t id)
{
#define UNKIDSTRLEN		50
	static char unknownstr[UNKIDSTRLEN];

	switch (id) {
	/* Device type attributes */
	case 0x0000: return "REMAINING CAPACITY IN PARTITION";
	case 0x0001: return "MAXIMUM CAPACITY IN PARTITION";
	case 0x0002: return "TAPEALERT FLAGS";
	case 0x0003: return "LOAD COUNT";
	case 0x0004: return "MAM SPACE REMAINING";
	case 0x0005: return "ASSIGNING ORGANIZATION";
	case 0x0006: return "FORMATTED DENSITY CODE";
	case 0x0007: return "INITIALIZATION COUNT";
	case 0x0008: return "VOLUME IDENTIFIER";
	case 0x0009: return "VOLUME CHANGE REFERENCE";
	case 0x020A: return "DEVICE VENDOR/SERIAL NUMBER AT LAST LOAD";
	case 0x020B: return "DEVICE VENDOR/SERIAL NUMBER AT LOAD-1";
	case 0x020C: return "DEVICE VENDOR/SERIAL NUMBER AT LOAD-2";
	case 0x020D: return "DEVICE VENDOR/SERIAL NUMBER AT LOAD-3";
	case 0x0220: return "TOTAL MBYTES WRITTEN IN MEDIUM LIFE";
	case 0x0221: return "TOTAL MBYTES READ IN MEDIUM LIFE";
	case 0x0222: return "TOTAL MBYTES WRITTEN IN CURRENT/LAST LOAD";
	case 0x0223: return "TOTAL MBYTES READ IN CURRENT/LAST LOAD";
	case 0x0224: return "LOGICAL POSITION OF FIRST ENCRYPTED BLOCK";
	case 0x0225: return "LOGICAL POSITION OF FIRST UNENCRYPTED BLOCK AFTER THE FIRST ENCRYPTED BLOCK";
	case 0x0340: return "MEDIUM USAGE HISTORY";
	case 0x0341: return "PARTITION USAGE HISTORY";
	/* Medium type attributes */
	case 0x0400: return "MEDIUM MANUFACTURER";
	case 0x0401: return "MEDIUM SERIAL NUMBER";
	case 0x0402: return "MEDIUM LENGTH";
	case 0x0403: return "MEDIUM WIDTH";
	case 0x0404: return "ASSIGNING ORGANIZATION";
	case 0x0405: return "MEDIUM DENSITY CODE";
	case 0x0406: return "MEDIUM MANUFACTURE DATE";
	case 0x0407: return "MAM CAPACITY";
	case 0x0408: return "MEDIUM TYPE";
	case 0x0409: return "MEDIUM TYPE INFORMATION";
	case 0x040A: return "NUMERIC MEDIUM SERIAL NUMBER";
	case 0x040B: return "SUPPORTED DENSITY CODES";
	/* Host type attributes */
	case 0x0800: return "APPLICATION VENDOR";
	case 0x0801: return "APPLICATION NAME";
	case 0x0802: return "APPLICATION VERSION";
	case 0x0803: return "USER MEDIUM TEXT LABEL";
	case 0x0804: return "DATE AND TIME LAST WRITTEN";
	case 0x0805: return "TEXT LOCALIZATION IDENTIFIER";
	case 0x0806: return "BARCODE";
	case 0x0807: return "OWNING HOST TEXTUAL NAME";
	case 0x0808: return "MEDIA POOL";
	case 0x0809: return "PARTITION USER TEXT LABEL";
	case 0x080A: return "LOAD/UNLOAD AT PARTITION";
	case 0x080B: return "APPLICATION FORMAT VERSION";
	case 0x080C: return "VOLUME COHERENCY INFORMATION";
	case 0x0820: return "MEDIUM GLOBALLY UNIQUE IDENTIFIER";
	case 0x0821: return "MEDIA POOL GLOBALLY UNIQUE IDENTIFIER";

	/* Vendor specific / non-standard */
	case 0x1000: return "UNIQUE CARTRIDGE IDENTITY";
	case 0x1001: return "ALTERNATIVE UNIQUE CARTRIDGE IDENTITY";
	}

	snprintf(unknownstr, UNKIDSTRLEN,
	    "Unknown or reserved attribute ID %x", id);

	return unknownstr;
}

static inline uint16_t
attribute_id_from_head(uint8_t *buf)
{
	return (buf[RDATTR_ATTRHEAD_ID_MSB] << 8) 
	    | buf[RDATTR_ATTRHEAD_ID_LSB];
}

static inline uint16_t
attribute_length_from_head(uint8_t *buf)
{
	return (buf[RDATTR_ATTRHEAD_ATTRLEN_MSB] << 8) 
	    | buf[RDATTR_ATTRHEAD_ATTRLEN_LSB];
}

static inline uint8_t
attribute_format_from_head(uint8_t *buf)
{
	return (buf[RDATTR_ATTRHEAD_FORMAT] & ATTR_FORMAT_MASK);
}

static inline bool
attribute_ro_from_head(uint8_t *buf)
{
	if (buf[RDATTR_ATTRHEAD_FORMAT] & ATTR_RO_MASK)
		return true;
	
	return false;
}

int
attribute_set_value(struct mam_attribute *ma, uint8_t *buf)
{
	size_t vbuflen;

	vbuflen = ma->length;

	if ((ma->format == ATTR_FORMAT_ASCII)
	    || (ma->format == ATTR_FORMAT_TEXT))
		(vbuflen)++;

	ma->value = GC_MALLOC(ma->length);

	if (ma->value == NULL) {
		fprintf(stderr, "Problem allocating memory for attribute.\n");
		return ENOMEM;
	}

	memset(ma->value, 0, vbuflen);
	memcpy(ma->value, buf+RDATTR_ATTRHEAD_LEN, ma->length);

	return 0;
}

static inline uint16_t
bswap16_to_host(uint16_t v)
{
if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
	return bswap16(v);
}

static inline uint32_t
bswap32_to_host(uint32_t v)
{
if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
	return bswap32(v);
}
static inline uint64_t
bswap64_to_host(uint64_t v)
{
if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
	return bswap64(v);
}

static char const *
attribute_value_to_string(struct mam_attribute *ma)
{
#define AVSTRLEN 255
	uint8_t i, cw;
	char *avstr;

	if ((ma->format) == ATTR_FORMAT_BINARY) {
		avstr = GC_MALLOC(AVSTRLEN);
		memset(avstr, 0, AVSTRLEN);

		switch (ma->length) {
		case 1:
			snprintf(avstr, AVSTRLEN, "%"PRIu8, *(ma->value));
			break;
		case 2:
			snprintf(avstr, AVSTRLEN, "%"PRIu16, bswap16_to_host(*(uint16_t *)(ma->value)));
			break;
		case 4:
			snprintf(avstr, AVSTRLEN, "%"PRIu32, bswap32_to_host(*(uint32_t *)(ma->value)));
			break;
		case 8:
			snprintf(avstr, AVSTRLEN, "%"PRIu64, bswap64_to_host(*(uint64_t *)(ma->value)));
			break;
		default: /* XXX */
			cw = 0;
			for (i = 0; i < ma->length; i++)
			{
				cw += snprintf(avstr+cw, AVSTRLEN-cw, "%02X", ma->value[i]);
			}
		}
	} else
		avstr = (char *) ma->value;

	return avstr;
}

void
attribute_print_simple(struct mam_attribute *ma)
{
	printf("%x %s (%s, %d bytes, %s):%s\n",
		ma->id,
		attribute_id_to_string(ma->id),
		attribute_format_to_string(ma->format),
		ma->length,
		attribute_ro_to_string(ma->ro),
		attribute_value_to_string(ma));
}

void
attribute_new(struct mam_attribute *ma, uint8_t *buf)
{
	ma->id = attribute_id_from_head(buf);
	ma->length = attribute_length_from_head(buf);
	ma->ro = attribute_ro_from_head(buf);
	ma->format = attribute_format_from_head(buf);
	ma->value = NULL;
}

void
cdb_list_attributes(scsicmd *cmd, uint8_t state, uint8_t length)
{
	memset(*cmd, 0, SCSI_CMD_LEN);

	(*cmd)[CDB_OPCODE]		= OP_READ_ATTRIBUTE;
	(*cmd)[CDB_RDATTR_SVCACTION]	= state;
	(*cmd)[CDB_RDATTR_ALLOCLEN_LSB]	= length;
}

void
cdb_read_attribute(scsicmd *cmd, uint16_t id, uint16_t length)
{
	memset(*cmd, 0, SCSI_CMD_LEN);

	(*cmd)[CDB_OPCODE]		= OP_READ_ATTRIBUTE;
	(*cmd)[CDB_RDATTR_ID_MSB]	= (id >> 8) & 0xff;
	(*cmd)[CDB_RDATTR_ID_LSB]	= id & 0xff;
	(*cmd)[CDB_RDATTR_ALLOCLEN_LSB]	= length;
}

int
mam_read_attribute_1(struct mam_attribute *ma, uint16_t id) 
{
	int error;
	//uint8_t i;
	uint8_t buf[256];
	uint8_t *bp;
	scsicmd cmd;

	memset(buf, 0, sizeof(buf));

	cdb_read_attribute(&cmd, id, RDATTR_HEADONLY_LEN);

	error = uscsi_command(SCSI_READCMD, &dev, cmd, 16, &buf,
	    RDATTR_HEADONLY_LEN, 10000, NULL);
	
	if (error)
		return error;

	bp = buf + RDATTR_LISTHEAD_LEN;

	attribute_new(ma, bp);

	cdb_read_attribute(&cmd, id, RDATTR_HEADONLY_LEN+(ma->length));

	error = uscsi_command(SCSI_READCMD, &dev, cmd, 16, &buf,
	    RDATTR_HEADONLY_LEN+(ma->length), 10000, NULL);

	if (error)
		return error;

	attribute_set_value(ma, bp);

	if (error)
		return error;

	if (ma->id != id)
		return EFAULT;


//	for(i = 0; i < 255; i++) {
//		printf("%x ", buf[i]);
//	}
//	printf("\n");

	return 0;

}

int
mam_list_attributes(uint8_t state)
{
	int error;
	uint8_t i;
	uint8_t *buf;
	uint16_t *bp;
	uint32_t bllen;
	scsicmd cmd;

	struct mam_attribute ma;

	buf = GC_MALLOC(RDATTR_LISTHEAD_LEN);
	//memset(buf, 0, RDATTR_LISTHEAD_LEN); // cleared by gc

	cdb_list_attributes(&cmd, state, RDATTR_LISTHEAD_LEN);
	error = uscsi_command(SCSI_READCMD, &dev, cmd, 16, buf,
	    RDATTR_LISTHEAD_LEN, 10000, NULL);

	if (error)
		return error;

	bllen = bswap32_to_host(*((uint32_t *) buf));

	buf = GC_MALLOC(RDATTR_LISTHEAD_LEN+bllen);

	cdb_list_attributes(&cmd, state, RDATTR_LISTHEAD_LEN+bllen);
	error = uscsi_command(SCSI_READCMD, &dev, cmd, 16, buf,
	    RDATTR_LISTHEAD_LEN+bllen, 10000, NULL);

	bp = (uint16_t *)(buf+4);

	//
	for (i = 0; i < (bllen/2); i++) {
		mam_read_attribute_1(&ma, bswap16_to_host(*(bp+i)));
		attribute_print_simple(&ma);
	}

	if (error)
		return error;
	
	return 0;
}

int
mam_scsi_device_open(char *dev_name) {

	int error;

	struct uscsi_addr saddr;

	dev.dev_name = dev_name;
	printf("Opening device %s\n", dev.dev_name);

	error = uscsi_open(&dev);
	if (error) {
		fprintf(stderr, "Device failed to open : %s\n",
		strerror(error));
		exit(1);
	}

	error = uscsi_check_for_scsi(&dev);
	if (error) {
		fprintf(stderr, "sorry, not a SCSI/ATAPI device : %s\n",
		    strerror(error));
		exit(1);
	}

	error = uscsi_identify(&dev, &saddr);
	if (error) {
		fprintf(stderr, "SCSI/ATAPI identify returned : %s\n",
		    strerror(error));
		exit(1);
	}

	printf("\nDevice identifies itself as : ");
	if (saddr.type == USCSI_TYPE_SCSI) {
		printf("SCSI   busnum = %d, target = %d, lun = %d\n",
		    saddr.addr.scsi.scbus, saddr.addr.scsi.target,
		    saddr.addr.scsi.lun);
	} else {
		printf("ATAPI  busnum = %d, drive = %d\n",
		    saddr.addr.atapi.atbus, saddr.addr.atapi.drive);
	}

	printf("\n");

	return error;
}


void
mam_scsi_device_close()
{
	uscsi_close(&dev);
}

int
main(int argc, char *argv[])
{
	int error;
	char *dev_name;
	//struct mam_attribute ma;

	GC_INIT();

//	while ((flag = getopt(argc, argv, "rf:v")) != -1) {

	if (flag_verbose)
		uscsilib_verbose = 1;

	dev_name = GC_strdup(default_tape);
	error = mam_scsi_device_open(dev_name);

	if (error) {
		fprintf(stderr, "Problem %x opening SCSI device %s\n",
		    error, dev_name);
		exit(1);
	}

	mam_list_attributes(ATTR_LIST_AVAILABLE);
//	mam_list_attributes(ATTR_LIST_SUPPORTED);
	/*
	mam_read_attribute_1(&ma, 0x000);
	attribute_print_simple(&ma);*/

	mam_scsi_device_close();

	return 0;

}

