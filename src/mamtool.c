#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <gc/gc.h>
#include <utlist.h>

#ifdef __linux__
#include <byteswap.h>
#endif

#include "uscsilib.h"

#define CDB_OPCODE		0
#define CDB_RDATTR_SVCACTION	1 
#define CDB_RDATTR_ID_MSB	8
#define CDB_RDATTR_ID_LSB	9
#define CDB_RDATTR_ALLOCLEN_LSB	13
#define CDB_WRATTR_PARAMLEN_LSB	CDB_RDATTR_ALLOCLEN_LSB

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

#define ATTR_FMT_MASK		0x03
#define ATTR_RO_MASK		0x80

#define ATTR_FMT_BINARY		0
#define ATTR_FMT_ASCII		1
#define ATTR_FMT_TEXT		2

struct mam_attribute {
	uint16_t id;
	uint16_t length;
	uint8_t format;
	bool ro;
	uint8_t *value;
};

struct mam_attribute_definition {
	uint16_t id;
	uint16_t lenght;
	uint8_t format;
	const char* name;
};

struct mam_id_list {
	uint16_t id;

	struct mam_id_list *next;
};

struct uscsi_dev dev;

#ifdef __linux__
static const char *default_tape = "/dev/nst0";
#else
static const char *default_tape = "/dev/enrst0";
#endif

#define ATTR_DEF_NUM 50
static struct mam_attribute_definition attr_def[] = {

	/* Device type attributes */
	{ 0x0000, 8, ATTR_FMT_BINARY, "REMAINING CAPACITY IN PARTITION" },
	{ 0x0001, 8, ATTR_FMT_BINARY, "MAXIMUM CAPACITY IN PARTITION"},
	{ 0x0002, 8, ATTR_FMT_BINARY, "TAPEALERT FLAGS"},
	{ 0x0003, 8, ATTR_FMT_BINARY, "LOAD COUNT" },
	{ 0x0004, 8, ATTR_FMT_BINARY, "MAM SPACE REMAINING" },
	{ 0x0005, 8, ATTR_FMT_ASCII, "ASSIGNING ORGANIZATION" },
	{ 0x0006, 1, ATTR_FMT_BINARY, "FORMATTED DENSITY CODE" },
	{ 0x0007, 2, ATTR_FMT_BINARY, "INITIALIZATION COUNT" },
	{ 0x0008, 32, ATTR_FMT_ASCII, "VOLUME IDENTIFIER" },
	{ 0x0009, 4, ATTR_FMT_BINARY, "VOLUME CHANGE REFERENCE" },
	{ 0x020A, 40, ATTR_FMT_ASCII, "DEVICE VENDOR/SERIAL NUMBER AT LAST LOAD" },
	{ 0x020B, 40, ATTR_FMT_ASCII, "DEVICE VENDOR/SERIAL NUMBER AT LOAD-1" },
	{ 0x020C, 40, ATTR_FMT_ASCII, "DEVICE VENDOR/SERIAL NUMBER AT LOAD-2" },
	{ 0x020D, 40, ATTR_FMT_ASCII, "DEVICE VENDOR/SERIAL NUMBER AT LOAD-3" },
	{ 0x0220, 8, ATTR_FMT_BINARY, "TOTAL MBYTES WRITTEN IN MEDIUM LIFE" },
	{ 0x0221, 8, ATTR_FMT_BINARY, "TOTAL MBYTES READ IN MEDIUM LIFE" },
	{ 0x0222, 8, ATTR_FMT_BINARY, "TOTAL MBYTES WRITTEN IN CURRENT/LAST LOAD" },
	{ 0x0223, 8, ATTR_FMT_BINARY, "TOTAL MBYTES READ IN CURRENT/LAST LOAD" },
	{ 0x0224, 8, ATTR_FMT_BINARY, "LOGICAL POSITION OF FIRST ENCRYPTED BLOCK" },
	{ 0x0225, 8, ATTR_FMT_BINARY, "LOGICAL POSITION OF FIRST UNENCRYPTED BLOCK AFTER THE FIRST ENCRYPTED BLOCK" },
	{ 0x0340, 90, ATTR_FMT_BINARY, "MEDIUM USAGE HISTORY" },
	{ 0x0341, 60, ATTR_FMT_BINARY, "PARTITION USAGE HISTORY" },
	/* Medium type attributes */
	{ 0x0400, 8, ATTR_FMT_ASCII, "MEDIUM MANUFACTURER" },
	{ 0x0401, 32, ATTR_FMT_ASCII, "MEDIUM SERIAL NUMBER" },
	{ 0x0402, 4, ATTR_FMT_BINARY, "MEDIUM LENGTH" },
	{ 0x0403, 4, ATTR_FMT_BINARY, "MEDIUM WIDTH" },
	{ 0x0404, 8, ATTR_FMT_ASCII, "ASSIGNING ORGANIZATION" },
	{ 0x0405, 1, ATTR_FMT_BINARY, "MEDIUM DENSITY CODE" },
	{ 0x0406, 8, ATTR_FMT_ASCII, "MEDIUM MANUFACTURE DATE" },
	{ 0x0407, 8, ATTR_FMT_BINARY, "MAM CAPACITY" },
	{ 0x0408, 1, ATTR_FMT_BINARY, "MEDIUM TYPE" },
	{ 0x0409, 2, ATTR_FMT_BINARY, "MEDIUM TYPE INFORMATION" },
	{ 0x040A, 0, ATTR_FMT_BINARY, "NUMERIC MEDIUM SERIAL NUMBER" }, /* XXX */
	{ 0x040B, 0, ATTR_FMT_BINARY, "SUPPORTED DENSITY CODES" }, /* XXX */
	/* Host type attributes */
	{ 0x0800, 8, ATTR_FMT_ASCII, "APPLICATION VENDOR" },
	{ 0x0801, 32, ATTR_FMT_ASCII, "APPLICATION NAME" },
	{ 0x0802, 8, ATTR_FMT_ASCII, "APPLICATION VERSION" },
	{ 0x0803, 160, ATTR_FMT_TEXT, "USER MEDIUM TEXT LABEL" },
	{ 0x0804, 12, ATTR_FMT_ASCII, "DATE AND TIME LAST WRITTEN" },
	{ 0x0805, 1, ATTR_FMT_BINARY, "TEXT LOCALIZATION IDENTIFIER" },
	{ 0x0806, 32, ATTR_FMT_ASCII, "BARCODE" },
	{ 0x0807, 80, ATTR_FMT_TEXT, "OWNING HOST TEXTUAL NAME" },
	{ 0x0808, 160, ATTR_FMT_TEXT, "MEDIA POOL" },
	{ 0x0809, 0, ATTR_FMT_TEXT, "PARTITION USER TEXT LABEL" }, /* XXX */
	{ 0x080A, 0, ATTR_FMT_BINARY, "LOAD/UNLOAD AT PARTITION" }, /* XXX */
	{ 0x080B, 16, ATTR_FMT_ASCII, "APPLICATION FORMAT VERSION" },
	{ 0x080C, 0, ATTR_FMT_BINARY, "VOLUME COHERENCY INFORMATION" },
	{ 0x0820, 36, ATTR_FMT_BINARY, "MEDIUM GLOBALLY UNIQUE IDENTIFIER" },
	{ 0x0821, 36, ATTR_FMT_BINARY, "MEDIA POOL GLOBALLY UNIQUE IDENTIFIER" },
	/* Vendor specific / non-standard */
	{ 0x1000, 28, ATTR_FMT_BINARY, "UNIQUE CARTRIDGE IDENTITY" },
	{ 0x1001, 24, ATTR_FMT_BINARY, "ALTERNATIVE UNIQUE CARTRIDGE IDENTITY" }
};

bool f_verbose = false;

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
	uint16_t i;
#define UNKIDSTRLEN		50
	static char unknownstr[UNKIDSTRLEN];

	for (i = 0; i <= ATTR_DEF_NUM; i++) {
		if (attr_def[i].id == id)
			return attr_def[i].name;
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
	return (buf[RDATTR_ATTRHEAD_FORMAT] & ATTR_FMT_MASK);
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

	if ((ma->format == ATTR_FMT_ASCII)
	    || (ma->format == ATTR_FMT_TEXT))
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
#ifdef __linux__
	return bswap_16(v);
#else
	return bswap16(v);
#endif
else
	return v;
}

static inline uint32_t
bswap32_to_host(uint32_t v)
{
if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#ifdef __linux__
	return bswap_32(v);
#else
	return bswap32(v);
#endif
else
	return v;
}
static inline uint64_t
bswap64_to_host(uint64_t v)
{
if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#ifdef __linux__
	return bswap_64(v);
#else
	return bswap64(v);
#endif
else
	return v;
}

static char const *
attribute_value_to_string(struct mam_attribute *ma)
{
#define AVSTRLEN 255
	uint16_t i, cw;
	char *avstr;

	if ((ma->format) == ATTR_FMT_BINARY) {
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

static inline void
attribute_print_value(struct mam_attribute *ma)
{
	printf("%s\n", attribute_value_to_string(ma));
}

static inline void
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

/*
 * Seralize the attribute to buffer that can be used with WRITE ATTRIBUTE
 * SCSI command.
 */
void
attribute_to_buffer(struct mam_attribute *ma, uint8_t *buf, uint32_t buflen)
{
	buf[3] = (uint8_t) buflen; //XXX
	buf[4] = (ma->id >> 8) & 0xFF ;
	buf[5] = ma->id & 0xFF;
	buf[6] = ma->format;
	buf[8] = (uint8_t) ma->length;

	// XXX: handle null termination of source value

	// attribute to buf
	// temporary PoC soluation
	//snprintf((char *)(buf+RDATTR_HEADONLY_LEN), ma->length, "%s",
	//    ma->value);
	strncpy((char *)(buf+RDATTR_HEADONLY_LEN), (const char *) ma->value, (ma->length));
}

void
uci_print_pretty(uint8_t *rawval) 
{
	uint32_t ltocm_serial;
	uint64_t pancake_id;
	char manufacturer[9];
	uint32_t lpos_lp1;
	uint16_t cartridge_type;

	memcpy(&ltocm_serial, rawval, 4);
	rawval += 4;
	memcpy(&pancake_id, rawval, 8);
	rawval += 8;
	memcpy(manufacturer, rawval, 8);
	rawval += 8;
	memcpy(&lpos_lp1, rawval, 4);
	rawval += 4;
	memcpy(&cartridge_type, rawval, 2);
	rawval += 4;

	printf("%u\n", bswap32_to_host(ltocm_serial));
	printf("%lu\n", bswap64_to_host(pancake_id));
	printf("%s\n", manufacturer);
	printf("%x\n", bswap32_to_host(lpos_lp1));
	printf("%x\n", bswap16_to_host(cartridge_type));

}

void
ucialt_print_pretty(uint8_t *rawval)
{

}

void
cdb_list_attributes(scsicmd *cmd, uint8_t state, uint32_t length)
{
	memset(*cmd, 0, SCSI_CMD_LEN);

	(*cmd)[CDB_OPCODE]		= OP_READ_ATTRIBUTE;
	(*cmd)[CDB_RDATTR_SVCACTION]	= state;
	/* XXX */
	(*cmd)[CDB_RDATTR_ALLOCLEN_LSB]	= length & 0xFF;
}

void
cdb_read_attribute(scsicmd *cmd, uint16_t id, uint32_t length)
{
	memset(*cmd, 0, SCSI_CMD_LEN);

	(*cmd)[CDB_OPCODE]		= OP_READ_ATTRIBUTE;
	(*cmd)[CDB_RDATTR_ID_MSB]	= (id >> 8) & 0xFF;
	(*cmd)[CDB_RDATTR_ID_LSB]	= id & 0xFF;
	/* XXX */
	(*cmd)[CDB_RDATTR_ALLOCLEN_LSB]	= length & 0xFF;
}

void
cdb_write_attribute(scsicmd *cmd, uint16_t id, uint32_t length)
{
	memset(*cmd, 0, SCSI_CMD_LEN);

	(*cmd)[CDB_OPCODE]		= OP_WRITE_ATTRIBUTE;
	/* XXX */
	(*cmd)[CDB_WRATTR_PARAMLEN_LSB]	= length & 0xFF;
}

int
mam_write_attribute_1(struct mam_attribute *ma)
{
	int error;
	uint8_t *buf;
	uint32_t buflen;
	scsicmd cmd;

	buflen = RDATTR_HEADONLY_LEN+(ma->length);
	buf = GC_MALLOC(buflen);

	cdb_write_attribute(&cmd, ma->id, buflen);

	attribute_to_buffer(ma, buf, buflen);

	error = uscsi_command(SCSI_WRITECMD, &dev, cmd, 16, buf,
	    buflen, 10000, NULL);
	
	if (error)
		return error;

	return 0;

}

int
mam_read_attribute_1(struct mam_attribute *ma, uint16_t id) 
{
	int error;
	uint8_t *buf;
	scsicmd cmd;

	buf = GC_MALLOC(RDATTR_HEADONLY_LEN);
	cdb_read_attribute(&cmd, id, RDATTR_HEADONLY_LEN);
	error = uscsi_command(SCSI_READCMD, &dev, cmd, 16, buf,
	    RDATTR_HEADONLY_LEN, 10000, NULL);
	
	if (error)
		return error;

	attribute_new(ma, buf + RDATTR_LISTHEAD_LEN);

	cdb_read_attribute(&cmd, id, RDATTR_HEADONLY_LEN+(ma->length));
	buf = GC_MALLOC(RDATTR_HEADONLY_LEN+(ma->length));
	error = uscsi_command(SCSI_READCMD, &dev, cmd, 16, buf,
	    RDATTR_HEADONLY_LEN+(ma->length), 10000, NULL);

	if (error)
		return error;

	attribute_set_value(ma, buf + RDATTR_LISTHEAD_LEN);

	if (error)
		return error;

	if (ma->id != id)
		return EFAULT;

	return 0;

}

/* XXX: ugly double pointer */
int
mam_list_attribute_ids(struct mam_id_list **list, uint8_t state)
{
	int error;
	uint16_t i;
	uint8_t *buf;
	uint16_t *bp;
	uint32_t bllen;
	scsicmd cmd;

	struct mam_id_list *lentry;

	buf = GC_MALLOC(RDATTR_LISTHEAD_LEN);
	assert(buf != NULL);

	cdb_list_attributes(&cmd, state, RDATTR_LISTHEAD_LEN);
	error = uscsi_command(SCSI_READCMD, &dev, cmd, 16, buf,
	    RDATTR_LISTHEAD_LEN, 10000, NULL);

	if (error)
		return error;

	bllen = bswap32_to_host(*((uint32_t *) buf));

	buf = GC_MALLOC(RDATTR_LISTHEAD_LEN+bllen);
	assert (buf != NULL);

	cdb_list_attributes(&cmd, state, RDATTR_LISTHEAD_LEN+bllen);
	error = uscsi_command(SCSI_READCMD, &dev, cmd, 16, buf,
	    RDATTR_LISTHEAD_LEN+bllen, 10000, NULL);

	bp = (uint16_t *)(buf+RDATTR_LISTHEAD_LEN);

	for (i = 0; i < bllen/2; i++) {
		lentry = GC_MALLOC(sizeof(struct mam_id_list));
		assert(lentry != NULL);
		lentry->id = bswap16_to_host(*(bp+i));
		LL_APPEND(*list, lentry);
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

	if (f_verbose)
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

	if (f_verbose) {
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
	}


	return error;
}


void
mam_scsi_device_close()
{
	uscsi_close(&dev);
}

/* Print all existing attributes. */
void
tool_dump_attributes()
{
	int error;
	struct mam_id_list *aid_list, *aid_entry;
	struct mam_attribute ma;

	aid_list = NULL;

	error = mam_list_attribute_ids(&aid_list, ATTR_LIST_AVAILABLE);

	if (error != 0) {
		fprintf(stderr, "Error obtaining attribute ID list "
		    "from SCSI device: %s\n"
		    "Use -v to get more information.\n", strerror(error));
		exit(EXIT_FAILURE);
	}

	LL_FOREACH(aid_list, aid_entry) {
		error = mam_read_attribute_1(&ma, aid_entry->id);
//		printf("id %x error %d\n", aid_entry->id, error);
		assert(error == 0); // XXX
		if (error == 0)
			attribute_print_simple(&ma);
	}
}

void
tool_write_attribute(char *strid, char *strformat, char *strvalue)
{
	char *ae;
	int error;
	struct mam_attribute ma;

	errno = 0;
	ma.id = (uint16_t) strtoul(strid, &ae, 0);
	assert(*ae == '\0');
	assert(errno == 0);

	ma.format = (uint8_t) strtoul(strformat, &ae, 0);
	assert(*ae == '\0');
	assert(errno == 0);

	ma.length = strlen(strvalue);

	printf("format %x, len of value %s is: %lx\n", ma.format, strvalue, strlen(strvalue));

	ma.value = (uint8_t *) strvalue;
	attribute_print_value(&ma);

	error = mam_write_attribute_1(&ma);
	if (error != 0) {
		fprintf(stderr, "Error writing attribute value "
		    "from SCSI device: %s\n"
		    "Use -v to get more information.\n", strerror(error));
		exit(EXIT_FAILURE);
	}

}

/* Read a single attribute. */
void
tool_read_attribute(char *strid)
{
	uint16_t aid;
	char *ae;
	int error;
	struct mam_attribute ma;

	errno = 0;
	aid = (uint16_t) strtoul(strid, &ae, 0);
	assert(*ae == '\0');
	assert(errno == 0);

	error = mam_read_attribute_1(&ma, aid);
	if (error != 0) {
		fprintf(stderr, "Error obtaining attribute value "
		    "from SCSI device: %s\n"
		    "Use -v to get more information.\n", strerror(error));
		exit(EXIT_FAILURE);
	}
	attribute_print_value(&ma);

}

/* Pretty print UCI data. */
void
tool_print_uci()
{
	int error;
	struct mam_attribute ma;

	error = mam_read_attribute_1(&ma, 0x1000);
	if (error != 0) {
		fprintf(stderr, "Error obtaining attribute value "
		    "from SCSI device: %s\n"
		    "Use -v to get more information.\n", strerror(error));
		exit(EXIT_FAILURE);
	}

	uci_print_pretty(ma.value);

}


static void
usage(const char *exec_name)
{
	fprintf(stderr, "%s [-f /dev/name] [-v] -L\n", exec_name);
	fprintf(stderr, "%s [-f /dev/name] [-v] -r attribute_ID\n", exec_name);
	fprintf(stderr, "%s [-f /dev/name] [-v] -w attribute_ID type value\n", exec_name);
	fprintf(stderr, "%s [-f /dev/name] [-v] -u\n", exec_name);
}

int
main(int argc, char *argv[])
{
	int error;
	const char *exec_name;
	char *dev_name;

	int flag = 0 ;
	int f_dump_attrs = 0;
	int f_read_attr = 0;
	int f_write_attr = 0;
	int f_uciprint = 0;

	GC_INIT();

#if defined(_NETBSD_SOURCE)
	exec_name = getprogname();
#else
	exec_name = GC_STRDUP(argv[0]);
#endif

	dev_name = GC_STRDUP(default_tape);

	if (argc < 2) {
		usage(exec_name);
		exit(EXIT_FAILURE);
	}

	while ((flag = getopt(argc, argv, "Lf:rwuv")) != -1) {
		switch (flag) {
			case 'L':
				f_dump_attrs = 1;
				break;
			case 'f':
				dev_name = GC_STRDUP(optarg);
				break;
			case 'r':
				f_read_attr = 1;
				break;
			case 'w':
				f_write_attr = 1;
				break;
			case 'u':
				f_uciprint = 1;
				break;
			case 'v':
				f_verbose = 1;
				break;
		}
	}

	argv += optind;
	argc -= optind;

	if (f_verbose)
		uscsilib_verbose = 1;

	error = mam_scsi_device_open(dev_name);

	if (error) {
		fprintf(stderr, "Problem %x opening SCSI device %s\n",
		    error, dev_name);
		exit(1);
	}

	if (f_dump_attrs) {
		tool_dump_attributes();
	}

	if (f_uciprint) {
		tool_print_uci();
	}

	if (f_read_attr) {
		if (argc < 1) {
			usage(exec_name);
			exit(EXIT_FAILURE);
		}

		tool_read_attribute(argv[0]);
	}

	if (f_write_attr) {
		if (argc < 3) {
			usage(exec_name);
			exit(EXIT_FAILURE);
		}
		tool_write_attribute(argv[0], argv[1], argv[2]);
	}

	mam_scsi_device_close();

	return EXIT_SUCCESS;

}

