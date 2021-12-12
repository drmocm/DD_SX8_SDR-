#include "dvb_print.h"
#include <stdarg.h>
static char table64[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
    'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
    'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
    'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
    'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
    'w', 'x', 'y', 'z', '0', '1', '2', '3',
    '4', '5', '6', '7', '8', '9', '+', '/'};

static int mod_table[] = {0, 2, 1};

static char *dtable() {

    char *dt = malloc(256);

    for (int i = 0; i < 64; i++)
        dt[(unsigned char) table64[i]] = i;
    return dt;
}

char *base64_encode(const uint8_t *data, int len, int *olen) {

    *olen = 4 * ((len + 2) / 3);

    char *edata = malloc(*olen);
    if (edata == NULL) return NULL;

    for (int i = 0, j = 0; i < len;) {

        uint32_t octet_a = i < len ? data[i++] : 0;
        uint32_t octet_b = i < len ? data[i++] : 0;
        uint32_t octet_c = i < len ? data[i++] : 0;

        uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;

        edata[j++] = table64[(triple >> 3 * 6) & 0x3F];
        edata[j++] = table64[(triple >> 2 * 6) & 0x3F];
        edata[j++] = table64[(triple >> 1 * 6) & 0x3F];
        edata[j++] = table64[(triple >> 0 * 6) & 0x3F];
    }

    for (int i = 0; i < mod_table[len % 3]; i++)
        edata[*olen - 1 - i] = '=';

    return edata;
}


unsigned char *base64_decode(uint8_t *data, int len, int *olen)
{
    
    char *dt = dtable();

    if (len % 4 != 0) return NULL;

    *olen = len / 4 * 3;
    if (data[len - 1] == '=') (*olen)--;
    if (data[len - 2] == '=') (*olen)--;

    unsigned char *ddata = malloc(*olen);
    if (ddata == NULL) return NULL;

    for (int i = 0, j = 0; i < len;) {

        uint32_t sextet_a = data[i] == '=' ? 0 & i++ : dt[data[i++]];
        uint32_t sextet_b = data[i] == '=' ? 0 & i++ : dt[data[i++]];
        uint32_t sextet_c = data[i] == '=' ? 0 & i++ : dt[data[i++]];
        uint32_t sextet_d = data[i] == '=' ? 0 & i++ : dt[data[i++]];

        uint32_t triple = (sextet_a << 3 * 6)
        + (sextet_b << 2 * 6)
        + (sextet_c << 1 * 6)
        + (sextet_d << 0 * 6);

        if (j < *olen) ddata[j++] = (triple >> 2 * 8) & 0xFF;
        if (j < *olen) ddata[j++] = (triple >> 1 * 8) & 0xFF;
        if (j < *olen) ddata[j++] = (triple >> 0 * 8) & 0xFF;
    }
    
    free(dt);
    return ddata;
}




void pr(int fd, const char  *format,  ...)
{
    va_list args;
    int print=0;
    char finalstr[4096];
    int w=0;
    char nfor[256];
    snprintf(nfor, sizeof(nfor), "%s", format);
    va_start(args, format);
    vsnprintf(finalstr,sizeof(finalstr),format,args);
    w=write(fd, finalstr, strlen(finalstr));
    va_end(args);
}

static void en300468_parse_string_to_utf8(char *dest, uint8_t *src,
				   const unsigned int len)
{
    int utf8 = (src[0] == 0x15) ? 1 : 0;
    int skip = (src[0] < 0x20) ? 1 : 0;
    if( src[0] == 0x10 ) skip += 2;
    uint16_t utf8_cc;
    int dest_pos = 0;
    int emphasis = 0;
    int i;
    
    for (i = skip; i < len; i++) {
	switch(*(src + i)) {
	case SB_CC_RESERVED_80 ... SB_CC_RESERVED_85:
	case SB_CC_RESERVED_88 ... SB_CC_RESERVED_89:
	case SB_CC_USER_8B ... SB_CC_USER_9F:
	case CHARACTER_CR_LF:
	    dest[dest_pos++] = '\n';
	    continue;
	case CHARACTER_EMPHASIS_ON:
	    emphasis = 1;
	    continue;
	case CHARACTER_EMPHASIS_OFF:
	    emphasis = 0;
	    continue;
	case UTF8_CC_START:
	    if (utf8 == 1) {
		utf8_cc = *(src + i) << 8;
		utf8_cc += *(src + i + 1);
		
		switch(utf8_cc) {
		case ((UTF8_CC_START << 8) | CHARACTER_EMPHASIS_ON):
		    emphasis = 1;
		    i++;
		    continue;
		case ((UTF8_CC_START << 8) | CHARACTER_EMPHASIS_OFF):
		    emphasis = 0;
		    i++;
		    continue;
		default:
		    break;
		}
	    }
	default: {
	    if (*(src + i) < 128)
		dest[dest_pos++] = *(src + i);
	    else {
		dest[dest_pos++] = 0xc2 + (*(src + i) > 0xbf);
		dest[dest_pos++] = (*(src + i) & 0x3f) | 0x80;
	    }
	    break;
	}
	}
    }
    dest[dest_pos] = '\0';
}

void dvb2txt(char *in)
{
    uint8_t len;
    char *out, *buf;
	
    len = strlen(in);
//    my_err(0,"%s len = %d",in, len);
    if (!len) return;
    buf = (char *) malloc(sizeof(char)*(len+1));

    if (!buf) {
	err("Error allocating memory\n");
	    exit(1);
    }
    memset(buf,0,len+1);
    out = buf;
    en300468_parse_string_to_utf8(out, (uint8_t *)in, len);
    
    int outlen = strlen(buf);
    memcpy(in,buf,sizeof(char)*outlen);
    in[outlen] = 0;
    free(buf);
}

void dvb_print_section(int fd, section *sec)
{
    int c=9;
    
    pr(fd,"section: table_id 0x%02x  syntax %d\n",
	    sec->table_id, sec->section_syntax_indicator);
    if (sec->section_syntax_indicator){
	pr(fd,"          id 0x%04x version_number 0x%02x\n" 
		"          section number: 0x%02x\n"
		"          last_section_number: 0x%02x\n",
		sec->id, sec->version_number, sec->section_number,
		sec->last_section_number);
	c+=5;
    }
    pr(fd,"        data (%d bytes):\n", sec->section_length+3);
    dvb_print_data(fd, sec->data,sec->section_length, 8,"          ", "");
    pr(fd,"\n");
}

void dvb_print_pat(int fd, PAT *pat)
{
    pr(fd,"PAT (0x%02x): transport_stream_id 0x%04x\n",
	    pat->pat->table_id, pat->pat->id);
    pr(fd,"  programs: \n");
    for(int n=0; n < pat->nprog; n++){
	pr(fd,"    program_number 0x%04x %s 0x%04x\n",
		pat->program_number[n],
		pat->program_number[n] ? "program_map_PID" : "network_PID",
		pat->pid[n]);
    }
    pr(fd,"\n");
}

const char *stream_type(uint8_t type)
{
    const char *t = "unknown";

    switch (type) {
    case 0x01:
	t = "video MPEG1";
	break;
    case 0x02:
	t = "video MPEG2";
	break;
    case 0x03:
	t = "audio MPEG1";
	break;
    case 0x04:
	t = "audio MPEG2";
	break;
    case 0x05:
	t = "MPEG-2 private data";
	break;
    case 0x06:
	t = "MPEG-2 packetized data (subtitles)";
	break;
    case 0x07:
	t = "MHEG";
	break;
    case 0x08:
	t = "ITU-T Rec. H.222.0 | ISO/IEC 13818-1 Annex A DSM-CC";
	break;
    case 0x09:
	t = "ITU-T Rec. H.222.1";
	break;
    case 0x0A:
	t = "DSM-CC ISO/IEC 13818-6 type A (Multi-protocol Encapsulation)";
	break;
    case 0x0B:
	t = "DSM-CC ISO/IEC 13818-6 type B (U-N messages)";
	break;
    case 0x0C:
	t = "DSM-CC ISO/IEC 13818-6 type C (Stream Descriptors)";
	break;
    case 0x0D:
	t = "DSM-CC ISO/IEC 13818-6 type D (Sections – any type)";
	break;
    case 0x0E:
	t = "ITU-T Rec. H.222.0 | ISO/IEC 13818-1 auxiliary";
	break;
    case 0x0F:
	t = "audio AAC";
	break;
    case 0x10:
	t = "video MPEG2";
	break;
    case 0x11:
	t = "audio LATM";
	break;
    case 0x12:
	t = "ISO/IEC 14496-1 SL-packetized stream or FlexMux stream carried in PES packets";
	break;
    case 0:
	t = "ISO/IEC 14496-1 SL-packetized stream or FlexMux stream carried in ISO/IEC14496_sections.";
	break;
    case 0x14:
	t = "ISO/IEC 13818-6 Synchronized Download Protocol";
	break;
    case 0x15:
	t = "Metadata in PES packets";
	break;
    case 0x16:
	t = "Metadata in metadata_sections";
	break;
    case 0x17:
	t = "Metadata 13818-6 Data Carousel";
	break;
    case 0x18:
	t = "Metadata 13818-6 Object Carousel";
	break;
    case 0x19:
	t = "Metadata 13818-6 Synchronized Download Protocol";
	break;
    case 0x1A:
	t = "IPMP (13818-11, MPEG-2 IPMP)";
	break;
    case 0x1B:
	t = "video H264 ISO/IEC 14496-10";
	break;
    case 0x1C:
	t = "audio ISO/IEC 14496-3 (DST, ALS and SLS)";
	break;
    case 0x1D:
	t = "text ISO/IEC 14496-17";
	break;
    case 0x1E:
	t = "video ISO/IEC 23002-3 Aux.";
	break;
    case 0x1F:
	t = "video ISO/IEC 14496-10 sub";
	break;
    case 0x20:
	t = "video MVC sub-bitstream";
	break;
    case 0x21:
	t = "video J2K";
	break;
    case 0x22:
	t = "video H.262 for 3D services";
	break;
    case 0x23:
	t = "video H.264 for 3D services";
	break;
    case 0x24:
	t = "video H.265 or HEVC temporal sub-bitstream";
	break;
    case 0x25:
	t = "video H.265 temporal subset";
	break;
    case 0x26:
	t = "video MVCD in AVC";
	break;
    case 0x42:
	t = "video CAVS";
	break;
    case 0x7F:
	t = "IPMP";
	break;
    case 0x81:
	t = "audio AC-3 (ATSC)";
	break;
    case 0x82:
	t = "audio DTS";
	break;
    case 0x83:
	t = "audio TRUEHD";
	break;
    case 0x86:
	t = "SCTE-35";
	break;
    case 0x87:
	t = "audio E-AC-3 (ATSC)";
	break;
    case 0xEA:
	t = "video VC1";
	break;
    case 0xD1:
	t = "video DIRAC";
	break;
    }
    return t;
}


void dvb_print_stream(int fd, pmt_stream *stream)
{
    pr(fd,"  stream: elementary_PID 0x%04x stream_type %s \n",
	    stream->elementary_PID, stream_type(stream->stream_type));
    if (stream->desc_num){
	uint32_t priv_id = 0;
	pr(fd,"  descriptors:\n");
	for (int n=0 ; n < stream->desc_num; n++){
	    priv_id = dvb_print_descriptor(fd, stream->descriptors[n],
					   "    ", priv_id);
	}
    }
}

void dvb_print_pmt(int fd, PMT *pmt)
{
    pr(fd,"PMT (0x%02x):  program_number 0x%04x  PCR_PID 0x%04x \n",
	    pmt->pmt->table_id, pmt->pmt->id, pmt->PCR_PID);
    if (pmt->desc_num) {
	pr(fd,"  program_info:\n");
	for (int n=0; n < pmt->desc_num; n++){
	    uint32_t priv_id = 0;
	    priv_id = dvb_print_descriptor(fd, pmt->descriptors[n],
					   "    ", priv_id);
	}
    }
    if (pmt->stream_num) {
	pr(fd,"  streams:\n");
	for (int n=0; n < pmt->stream_num; n++){
	    dvb_print_stream(fd, pmt->stream[n]);
	}
    }
    pr(fd,"\n");
}

void dvb_print_transport(int fd, nit_transport *trans)
{
    pr(fd,"  transport:\n"
	    "    transport_stream_id 0x%04x original_network_id 0x%04x\n",
	    trans->transport_stream_id, trans->original_network_id);
    if (trans->desc_num){
	uint32_t priv_id = 0;
	pr(fd,"    descriptors:\n");
	for (int n=0 ; n < trans->desc_num; n++){
	    priv_id = dvb_print_descriptor(fd, trans->descriptors[n],
					   "      ", priv_id);
	}
    }
}

void dvb_print_nit(int fd, NIT *nit)
{
    pr(fd,"NIT (0x%02x): %s network (%d/%d) \n  network_id 0x%04x\n",
	    nit->nit->table_id, 
	    (nit->nit->table_id == 0x41) ? "other":"actual", 
	    nit->nit->section_number+1,
	    nit->nit->last_section_number+1, nit->nit->id);
    if (nit->ndesc_num){
	uint32_t priv_id = 0;
	pr(fd,"  network descriptors:\n");
	for (int n=0 ; n < nit->ndesc_num; n++){
	    priv_id = dvb_print_descriptor(fd, nit->network_descriptors[n],
					   "    ", priv_id);
	}
    }
    if (nit->trans_num){
	pr(fd,"  transports:\n");
	for (int n=0 ; n < nit->trans_num; n++){
	    dvb_print_transport(fd, nit->transports[n]);
	}
    }
    pr(fd,"\n");
}

void dvb_print_service(int fd, sdt_service *serv)
{
    const char *R[] = {	"undefined","not running",
	"starts in a few seconds","pausing","running","service off-air",
	"unknown","unknown"};
    
    pr(fd,"  service:\n"
	    "    service_id 0x%04x EIT_schedule_flag 0x%02x\n"
	    "    EIT_present_following_flag 0x%02x\n"
	    "    running_status %s free_CA_mode 0x%02x\n",
	    serv->service_id, serv->EIT_schedule_flag,
	    serv->EIT_present_following_flag, R[serv->running_status],
	    serv->free_CA_mode);
    if (serv->desc_num){
	uint32_t priv_id = 0;
	pr(fd,"    descriptors:\n");
	for (int n=0 ; n < serv->desc_num; n++){
	    priv_id = dvb_print_descriptor(fd, serv->descriptors[n], "      ",
					   priv_id);
	}
    }
}

void dvb_print_sdt(int fd, SDT *sdt)
{
    pr(fd,"SDT (0x%02x): (%d/%d)\n  original_network_id 0x%04x ",
	    sdt->sdt->table_id, sdt->sdt->id, sdt->sdt->section_number,
	    sdt->sdt->last_section_number);
    if (sdt->service_num){
	pr(fd,"  services:\n");
	for (int n=0 ; n < sdt->service_num; n++){
	    dvb_print_service(fd, sdt->services[n]);
	}
    }
}

void dvb_print_delsys_descriptor(int fd, descriptor *desc, char *s)
{
    uint8_t *buf = desc->data;
    uint32_t freq;
    uint16_t orbit;
    uint32_t srate;
    uint8_t pol;
    uint8_t delsys;
    uint8_t mod;
    uint8_t fec;
    uint8_t east;
    uint8_t roll;

    const char *POL[] = {"linear-horizontal", "linear-vertical",
	"circular-left", "circulra-right"};
    const char *MOD[] = {"Auto", "QPSK", "8PSK", "16QAM"};
    const char *MODC[] ={"not defined","16-QAM","32-QAM","64-QAM",
	"128-QAM","256-QAM","reserved"};
    const double roff[] ={0.25, 0.35, 0.20, 0};
    const char *FECO[] ={"not defined","no outer FEC coding",
	"RS(204/188)","reserved"};
    const char *FEC[] ={"not defined", "1/2" ,"2/3", "3/4","5/6","7/8","8/9",
	"3/5","4/5","9/10","reserved","no conv. coding"};
	
    switch(desc->tag){
    case 0x43: // satellite
	pr(fd,"%s  Satellite delivery system descriptor: \n",s);
	freq = getbcd(buf, 8) *10;
	orbit = getbcd(buf+4, 4) *10;
	srate = getbcd(buf + 7, 7) / 10;
	east = ((buf[6] & 0x80) >> 7);
	pol =  ((buf[6] & 0x60) >> 5); 
	roll = ((buf[6] & 0x18) >> 3);
	delsys = ((buf[6] & 0x04) >> 2) ? SYS_DVBS2 : SYS_DVBS;
	mod = buf[6] & 0x03;
	fec = buf[10] & 0x0f;
	if (fec > 10 && fec < 15) fec = 10;
	if (fec == 15) fec = 11;
	pr(fd,
		"%s  frequency %d orbital_position %d west_east_flag %s\n"
		"%s  polarization %s  modulation_system %s",s,
		freq, orbit, east ? "E":"W", s, POL[pol],
		delsys ? "DVB-S2":"DVB-S");
	if (delsys) pr(fd," roll_off %.2f\n", roff[roll]);
	pr(fd,
		"%s  modulation_type %s symbol_rate %d FEC_inner %s\n",s, 
		MOD[mod], srate, FEC[fec]);
	break;

    case 0x44: // cable
	pr(fd,"%s  Cable delivery system descriptor\n",s);

	freq =  getbcd(buf, 8)/10;
	delsys = buf[5] & 0x0f;
	mod = buf[6];
	if (mod > 6) mod = 6;
	srate = getbcd(buf + 7, 7) / 10;
	fec = buf[10] & 0x0f;
	if (fec > 10 && fec < 15) fec = 10;
	if (fec == 15) fec = 11;
	pr(fd,
		"%s  frequency %d FEC_outer %s modulation %s\n"
		"%s  symbol_rate %d FEC_inner %s\n",
		s, freq, FECO[delsys],MODC[mod],s, srate, FEC[fec]);
	break;

    case 0x5a: // terrestrial
	freq = (buf[5]|(buf[4] << 8)|(buf[3] << 16)|(buf[0] << 24))*10;
	delsys = SYS_DVBT;
	break;

    case 0xfa: // isdbt
	freq = (buf[5]|(buf[4] << 8))*7000000;
	delsys = SYS_ISDBT;
    }
}

static char *dvb_get_name(uint8_t *buf, int len)
{
    char *name=NULL;
    if (len){
	name = malloc(sizeof(char)*len+1);
	memset(name,0,len+1);
	memcpy(name,buf,len);
	dvb2txt(name);
    }
    return name;
}

void dvb_print_data(int fd, uint8_t *b, int length, int step,
		    char *s, char *s2)
{
    int i, j, n;
    if(!step) step = 16;
    if (!length) return;
    n = 0;
    for (j = 0; j < length; j += step, b += step) {
	pr(fd,"%s%s  %03d: ",s,s2,n);
	for (i = 0; i < step; i++)
	    if (i + j < length)
		pr(fd,"0x%02x ", b[i]);
	    else
		pr(fd,"     ");
	pr(fd," | ");
	for (i = 0; i < step; i++)
	    if (i + j < length)
		pr(fd,"%c",(b[i] > 31 && b[i] < 127) ? b[i] : '.');
	pr(fd,"\n");
	n++;
    }
    
}

void dvb_print_linkage_descriptor(int fd, descriptor *desc, char *s)
{
    uint16_t nid = 0;
    uint16_t onid = 0;
    uint16_t sid = 0;
    uint16_t tsid = 0;
    uint8_t link =0;
    uint8_t *buf = desc->data;
    int length = desc->len;
    int c = 0;
    const char *H[] = {
	"reserved",
	"DVB hand-over to an identical service in a neighbouring country",
	"DVB hand-over to a local variation of the same service",
	"DVB hand-over to an associated service",
	"reserved"
    };
	
    const char *L[] = {
	"reserved","information service","EPG service",
	"CA replacement service",
	"TS containing complete Network/Bouquet SI",
	"service replacement service",
	"data broadcast service","RCS Map","mobile hand-over",
	"System Software Update Service (TS 102 006 [11])",
	"TS containing SSU BAT or NIT (TS 102 006 [11])",
	"IP/MAC Notification Service (EN 301 192 [4])",
	"TS containing INT BAT or NIT (EN 301 192 [4])"};

    pr(fd,"%s  Linkage descriptor:\n",s);
    tsid = (buf[0] << 8) | buf[1];
    onid = (buf[2] << 8) | buf[3];
    sid = (buf[4] << 8) | buf[5];
    link = buf[6];
    
    const char *lk = NULL;
    if (link < 0x0D) lk = L[link];
    else if(0x80 < link && link < 0xff) lk="user defined"; 
    else lk="reserved";
    
    pr(fd,
	    "%s    transport_stream_id 0x%04x original_network_id 0x%04x\n"
	    "%s    service_id 0x%04x linkage_type \"%s\"\n",s,tsid,onid,s,
	    sid, lk);
		

    c = 7;
    if (link == 0x08){
	uint8_t hand = (buf[c]&0xf0)>>4;
	uint8_t org = buf[c]&0x01;
	pr(fd,
		"%s    handover_type %s origin_type %s\n",s,
		H[hand], org ? "SDT":"NIT");
	if (hand ==0x01 || hand ==0x02 || hand ==0x03){
	    nid = (buf[c+1] << 8) | buf[c+2];
	    pr(fd,
		    "%s    network_id 0x%04x\n",s,nid);
	    c++;
	}
	if (!org){
	    sid = (buf[c+1] << 8) | buf[c+2];
	    pr(fd,
		    "%s    initial_service_id 0x%04x\n",s,sid);
	    c++;
	}
    }
    buf += c;
    length -= c;
    if (length){
	pr(fd,"%s    private_data_bytes: %d %s\n",s, length,
		length>1 ? "bytes":"byte");
	dvb_print_data(fd, buf, length, 8,  s, "    ");
    }
}

static const char *service_type(uint8_t type)
{
    const char *t = "unknown";

    switch (type) {

    case 0x00:
    case 0x20 ... 0x7F:
    case 0x12 ... 0x15:
    case 0xFF:
	t = "reserved";
	break;
    case 0x01:
	t = "digital television service";
	break;
    case 0x02:
	t = "digital radio sound service";
	break;
    case 0x03:
	t = "Teletext service";
	break;
    case 0x04:
	t = "NVOD reference service";
	break;
    case 0x05:
	t = "NVOD time-shifted service";
	break;
    case 0x06:
	t = "mosaic service";
	break;
    case 0x07:
	t = "PAL coded signal";
	break;
    case 0x08:
	t = "SECAM coded signal";
	break;
    case 0x09:
	t = "D/D2-MAC";
	break;
    case 0x0A:
	t = "FM Radio";
	break;
    case 0x0B:
	t = "NTSC coded signal";
	break;
    case 0x0C:
	t = "data broadcast service";
	break;
    case 0x0D:
	t = "reserved for Common Interface usage";
	break;
    case 0x0E:
	t = "RCS Map (see EN 301 790)";
	break;
    case 0x0F:
	t = "RCS FLS (see EN 301 790)";
	break;
    case 0x10:
	t = "DVB MHP service";
	break;
    case 0x11:
	t = "MPEG-2 HD digital television service";
	break;
    case 0x16:
	    t = "H.264/AVC SD digital television service";
	break;
    case 0x17:
	    t = "H.264/AVC SD NVOD time-shifted service";
	break;
    case 0x18:
	    t = "H.264/AVC SD NVOD reference service";
	break;
    case 0x19:
	    t = "H.264/AVC HD digital television service";
	break;
    case 0x1A:
	    t = "H.264/AVC HD NVOD time-shifted service";
	break;
    case 0x1B:
	    t = "H.264/AVC HD NVOD reference service";
	break;
    case 0x1C:
	    t = "H.264/AVC frame compatible plano-stereoscopic HD digital television service ";
	break;
    case 0x1D:
	    t = "H.264/AVC frame compatible plano-stereoscopic HD NVOD time-shifted service";
	break;
    case 0x1E:
	    t = "H.264/AVC frame compatible plano-stereoscopic HD NVOD reference service";
	break;
    case 0x1F:
	    t = "HEVC digital television service";
	break;
    case 0x80 ... 0xFE:
	t = "user defined";
	break;
    }
    return t;
}


uint32_t dvb_print_descriptor(int fd, descriptor *desc, char *s,
			      uint32_t priv_id)
{
    uint8_t *buf = desc->data;
    int c = 0;
    char *name=NULL;
    uint16_t id;

    
    pr(fd,"%sDescriptor tag: 0x%02x \n",s,desc->tag);
    switch(desc->tag){
    case 0x40:// network_name_descriptor
	pr(fd,"%s  Network name descriptor: \n",s);
	if ((name = dvb_get_name(buf,desc->len))){
	    pr(fd,"%s  name %s\n",s, name);
	    free(name);
	}
	break;
    case 0x41: //service list
	pr(fd,"%s  Service list descriptor:\n",s);
	for (int n = 0; n < desc->len; n+=3){
	    id = (buf[n] << 8) | buf[n+1];
	    pr(fd,"%s    service_id 0x%04x service_type %s\n",s, id,
		    service_type(buf[n+2]));
	}
	break;
    case 0x43: // satellite
	dvb_print_delsys_descriptor(fd, desc, s);
	break;
    
    case 0x44: // cable
	dvb_print_delsys_descriptor(fd, desc, s);
	break;

    case 0x48: //service descriptor
	pr(fd,"%s  Service descriptor:\n",s,desc->tag);
	pr(fd,"%s    service_type %s\n",s,service_type(buf[0])); 
	c++;
	int l = buf[c];
	c++;
	if ((name = dvb_get_name(buf+c,l))){
	    pr(fd,"%s    provider %s",s, name);
	    free(name);
	}
	c += l;
	l = buf[c];
	c++;
	if ((name = dvb_get_name(buf+c,l))){
	    pr(fd," name %s ", name);
	    free(name);
	}
	pr(fd,"\n");
	break;

    case 0x4a:
	dvb_print_linkage_descriptor(fd, desc, s);
	break;
	
    case 0x5a: // terrestrial
	dvb_print_delsys_descriptor(fd, desc, s);
	break;

    case 0x5f:
	pr(fd,"%s  Private data specifier descriptor: \n",s);
	priv_id = (buf[0]<<24)|(buf[1]<<16)|(buf[2]<<8)|buf[3];
	pr(fd,"%s    private_data_specifier 0x%08x\n",s,priv_id);
	break;

    case 0x7f:
	pr(fd,"%s  Extension descriptor: \n",s);
	pr(fd,"%s    length: %d %s\n",s, desc->len,
		desc->len>1 ? "bytes":"byte");
	dvb_print_data(fd, desc->data, desc->len, 8, s, "  ");
	break;
	    
    case 0xfa: // isdbt
	dvb_print_delsys_descriptor(fd, desc, s);
	break;

    case 0xfb ... 0xfe:
    case 0x80 ... 0xf9: // user defined
	switch (priv_id){
	case NORDIG:
	    switch (desc->tag){
	    case 0x83:
	    case 0x87:
		pr(fd,"%s  NorDig Logical channel descriptor: \n",s);
		for (int n = 0; n < desc->len; n+=3){
		    id = (buf[n] << 8) | buf[n+1];
		    uint16_t lcn = ((buf[n+2]&0x3f) << 8) | buf[n+3];
		    pr(fd,
			    "%s    service_id 0x%04x logical channel number %d\n",
			    s, id, lcn);
		}
		break;
	    default:
		pr(fd,"%s  NorDig defined: \n",s);
		pr(fd,"%s    length: %d %s\n",s, desc->len,
			desc->len>1 ? "bytes":"byte");
		dvb_print_data(fd, desc->data, desc->len, 8, s, "  ");
		break;
	    }
	    break;

	default:
	    pr(fd,"%s  User defined descriptor:\n",s);
	    pr(fd,"%s    length: %d %s\n",s, desc->len,
		    desc->len>1 ? "bytes":"byte");
	    dvb_print_data(fd, desc->data, desc->len, 8, s, "  ");
	    break;
	}
	break;
    default:
	pr(fd,"%s  UNHANDLED descriptor: \n",s);
	pr(fd,"%s    length: %d %s\n",s, desc->len,
		desc->len>1 ? "bytes":"byte");
	dvb_print_data(fd, desc->data, desc->len, 8, s, "  ");
	break;

    }
    return priv_id;
}


static json_object *dvb_data_json(uint8_t *data, int len)
{
    int olen = 0;
    char *sdata = base64_encode(data, len,&olen);

    return json_object_new_string_len(sdata,len);
}

json_object *dvb_section_json(section *sec, int d)
{
    json_object *jobj = json_object_new_object();
    json_object *jarray;

    int c=9;
    
    json_object_object_add(jobj, "table_id",
			   json_object_new_int(sec->table_id));
    json_object_object_add(jobj, "syntax_indicator",
			   json_object_new_int(sec->section_syntax_indicator));
    json_object_object_add(jobj,"length",
			   json_object_new_int(sec->section_length));
    if(d) {
	json_object_object_add(jobj,"data",
			       dvb_data_json(sec->data, sec->section_length));
    }
    if (sec->section_syntax_indicator){
	json_object_object_add(jobj, "section_number",
			   json_object_new_int(sec->section_number));
	json_object_object_add(jobj, "last_section_number",
			       json_object_new_int(sec->last_section_number));
	json_object_object_add(jobj, "version_number",
			       json_object_new_int(sec->version_number));
	if(d) {
	    json_object_object_add(jobj, "id",
				   json_object_new_int(sec->id));
	}
    }
    return jobj;
}

json_object *dvb_linkage_descriptor_json(descriptor *desc)
{
    uint16_t nid = 0;
    uint16_t onid = 0;
    uint16_t sid = 0;
    uint16_t tsid = 0;
    uint8_t link =0;
    uint8_t *buf = desc->data;
    int length = desc->len;
    int c = 0;
    const char *H[] = {
	"reserved",
	"DVB hand-over to an identical service in a neighbouring country",
	"DVB hand-over to a local variation of the same service",
	"DVB hand-over to an associated service",
	"reserved"
    };
	
    const char *L[] = {
	"reserved","information service","EPG service",
	"CA replacement service",
	"TS containing complete Network/Bouquet SI",
	"service replacement service",
	"data broadcast service","RCS Map","mobile hand-over",
	"System Software Update Service (TS 102 006 [11])",
	"TS containing SSU BAT or NIT (TS 102 006 [11])",
	"IP/MAC Notification Service (EN 301 192 [4])",
	"TS containing INT BAT or NIT (EN 301 192 [4])"};

    json_object *jobj = json_object_new_object();
    json_object_object_add(jobj,"type", json_object_new_string(
			       "Linkage descriptor"));
    tsid = (buf[0] << 8) | buf[1];
    onid = (buf[2] << 8) | buf[3];
    sid = (buf[4] << 8) | buf[5];
    link = buf[6];
    
    const char *lk = NULL;
    if (link < 0x0D) lk = L[link];
    else if(0x80 < link && link < 0xff) lk="user defined"; 
    else lk="reserved";
    
    json_object_object_add(jobj,"transport_stream_id",
			   json_object_new_int(tsid));
    json_object_object_add(jobj,"original_network_id",
			   json_object_new_int(onid));
    json_object_object_add(jobj,"service id",
			   json_object_new_int(sid));
    json_object_object_add(jobj,"linkage_type nr",
			   json_object_new_int(link));
    json_object_object_add(jobj,"linkage_type",
			   json_object_new_string(lk));
    
    c = 7;
    if (link == 0x08){
	uint8_t hand = (buf[c]&0xf0)>>4;
	uint8_t org = buf[c]&0x01;
	json_object_object_add(jobj,"handover_type",
			       json_object_new_string(H[hand]));
	json_object_object_add(jobj,"handover_type nr",
			       json_object_new_int(hand));
	json_object_object_add(jobj,"handover_type",
			       json_object_new_string((org ? "SDT":"NIT")));
	json_object_object_add(jobj,"origin_type nr",
			       json_object_new_int(org));
	if (hand ==0x01 || hand ==0x02 || hand ==0x03){
	    nid = (buf[c+1] << 8) | buf[c+2];
	    json_object_object_add(jobj,"network_id",
				   json_object_new_int(nid));
	    c++;
	}
	if (!org){
	    sid = (buf[c+1] << 8) | buf[c+2];
	    json_object_object_add(jobj,"initia_ service_id",
				   json_object_new_int(sid));
	    c++;
	}
    }
    buf += c;
    length -= c;
    if (length){
	json_object_object_add(jobj,"length",
			       json_object_new_int(length));
	json_object_object_add(jobj,"data",
			       dvb_data_json(buf, length));
    }
    return jobj;
}

json_object *dvb_descriptor_json(descriptor *desc, uint32_t *priv_id)
{
    uint8_t *buf = desc->data;
    int c = 0;
    char *name=NULL;
    uint16_t id;
    json_object *jobj = json_object_new_object();
    json_object *jarray;

    json_object_object_add(jobj,"tag",
			   json_object_new_int(desc->tag));
    switch(desc->tag){
    case 0x40:// network_name_descriptor
	json_object_object_add(jobj,"type",
			       json_object_new_string(
				   "Network name descriptor"));
	if ((name = dvb_get_name(buf,desc->len))){
	    json_object_object_add(jobj,"name",
				   json_object_new_string(name));
	    free(name);
	}
	break;
    case 0x41: //service list
	json_object_object_add(jobj,"type",
			       json_object_new_string(
				   "Service list descriptor"));
	jarray = json_object_new_array();

	for (int n = 0; n < desc->len; n+=3){
	    json_object *ja = json_object_new_object();
	    id = (buf[n] << 8) | buf[n+1];
	    json_object_object_add(ja, "service id",
				   json_object_new_int( id));
	    json_object_object_add(ja, "service type nr",
				   json_object_new_int( buf[n+2]));
	    json_object_object_add(ja, "service type",
				   json_object_new_string(
				       service_type(buf[n+2])));
	    json_object_array_add(jarray, ja);
	}
	json_object_object_add(jobj, "Services", jarray);
	break;
    case 0x43: // satellite
//	dvb_print_delsys_descriptor(fd, desc, s);
	break;
    
    case 0x44: // cable
//	dvb_print_delsys_descriptor(fd, desc, s);
	break;

    case 0x48: //service descriptor
	json_object_object_add(jobj,"type",
			       json_object_new_string(
				   "Service descriptor"));
	
	json_object_object_add(jobj,"service type",
			       json_object_new_string(
				   service_type(buf[0]))); 
	json_object_object_add(jobj,"service type nr",
			       json_object_new_int(buf[0])); 
	c++;
	int l = buf[c];
	c++;
	if ((name = dvb_get_name(buf+c,l))){
	    json_object_object_add(jobj,"provider",
				   json_object_new_string(name));
	    free(name);
	}
	c += l;
	l = buf[c];
	c++;
	if ((name = dvb_get_name(buf+c,l))){
	    json_object_object_add(jobj,"name",
				   json_object_new_string(name));
	    free(name);
	}
	break;

    case 0x4a:
	json_object_put(jobj);
	jobj = dvb_linkage_descriptor_json(desc);
	break;
	
    case 0x5a: // terrestrial
//	dvb_print_delsys_descriptor(fd, desc, s);
	break;

    case 0x5f:
	json_object_object_add(jobj,"type",
			       json_object_new_string(
				   "Private data specifier descriptor"));
	
	*priv_id = (buf[0]<<24)|(buf[1]<<16)|(buf[2]<<8)|buf[3];
	json_object_object_add(jobj,"private_data_specifier",
			       json_object_new_int(*priv_id));
	break;

    case 0x7f:
	json_object_object_add(jobj,"type",
			       json_object_new_string(
				   "Extension descriptor"));
	json_object_object_add(jobj,"length",
			       json_object_new_int(desc->len));
	json_object_object_add(jobj,"data",
			       dvb_data_json(desc->data, desc->len));
	break;
	    
    case 0xfa: // isdbt
//	dvb_print_delsys_descriptor(fd, desc, s);
	break;

    case 0xfb ... 0xfe:
    case 0x80 ... 0xf9: // user defined
	switch (*priv_id){
	case NORDIG:
	    switch (desc->tag){
	    case 0x83:
	    case 0x87:
		json_object_object_add(jobj,"type",
				       json_object_new_string(
					   "NorDig Logical channel descriptor"));
		jarray = json_object_new_array();
		
		for (int n = 0; n < desc->len; n+=3){
		    id = (buf[n] << 8) | buf[n+1];
		    uint16_t lcn = ((buf[n+2]&0x3f) << 8) | buf[n+3];
		    json_object *ja = json_object_new_object();
		    json_object_object_add(ja, "service_id",
					   json_object_new_int(id));
		    json_object_object_add(ja, "logical channel number",
					   json_object_new_int( lcn));
		    json_object_array_add(jarray,ja);
		}
		break;
	    default:
		json_object_object_add(jobj,"type",
				       json_object_new_string(
					   "NorDig private data"));
		json_object_object_add(jobj,"length",
				       json_object_new_int(desc->len));
		json_object_object_add(jobj,"data",
				       dvb_data_json(desc->data, desc->len));
		break;
	    }
	    break;

	default:
	    json_object_object_add(jobj,"type",
				   json_object_new_string(
				       "User defined descriptor"));
	    json_object_object_add(jobj,"length",
				   json_object_new_int(desc->len));
	    json_object_object_add(jobj,"data",
				   dvb_data_json(desc->data, desc->len));
	    break;
}
	break;
    default:
	json_object_object_add(jobj,"type",
			       json_object_new_string(
				   "UNHANDLED descriptor"));
	json_object_object_add(jobj,"length",
			       json_object_new_int(desc->len));
	json_object_object_add(jobj,"data",
			       dvb_data_json(desc->data, desc->len));
	break;
	
    }
    return jobj;
}

json_object *dvb_pat_json(PAT *pat)
{
    json_object *jobj = json_object_new_object();
    json_object *jarray;

    json_object_object_add(jobj, "section data", dvb_section_json(pat->pat,0));
    json_object_object_add(jobj, "transport_stream_id",
			   json_object_new_int(pat->pat->id));
    jarray = json_object_new_array();
    for(int n=0; n < pat->nprog; n++){
	if (pat->program_number[n]){
	    json_object *ja = json_object_new_object();
	    json_object_object_add(ja, "program_number",
				   json_object_new_int(pat->program_number[n]));
	    json_object_object_add(ja, "program_map_PID",
				   json_object_new_int( pat->pid[n]));
	    
	    json_object_array_add(jarray,ja);
	} else {
	    json_object_object_add(jobj, "network_id",
				   json_object_new_int( pat->pid[n]));
	}
    }
    json_object_object_add(jobj, "programs", jarray);
    return jobj;
}

json_object *dvb_stream_json(pmt_stream *stream)
{
    json_object *jobj = json_object_new_object();
    json_object *jarray;

    json_object_object_add(jobj, "elementary_PID",
			   json_object_new_int(stream->elementary_PID));
    json_object_object_add(jobj, "stream_type_nr",
			   json_object_new_int(stream->stream_type));
    json_object_object_add(jobj, "stream_type",
			   json_object_new_string(
			       stream_type(stream->stream_type)));
    if (stream->desc_num){
	uint32_t priv_id = 0;
	jarray = json_object_new_array();

	for (int n=0 ; n < stream->desc_num; n++){
	    json_object_array_add(jarray,
				  dvb_descriptor_json(stream->descriptors[n],
						      &priv_id));
	}
	json_object_object_add(jobj, "descriptors", jarray);
    }
    return jobj;
}

json_object *dvb_pmt_json(PMT *pmt)
{
    json_object *jobj = json_object_new_object();
    json_object *jarray;

    json_object_object_add(jobj, "section data", dvb_section_json(pmt->pmt,0));
    json_object_object_add(jobj, "program_number",
			   json_object_new_int(pmt->pmt->id));
    json_object_object_add(jobj, "PCR_PID",
			   json_object_new_int(pmt->PCR_PID));

    if (pmt->desc_num) {
	jarray = json_object_new_array();
	uint32_t priv_id = 0;

	for (int n=0 ; n < pmt->desc_num; n++){
 	    json_object_array_add(jarray,
				  dvb_descriptor_json(pmt->descriptors[n],
						      &priv_id));
	}
	json_object_object_add(jobj, "program info descriptors", jarray);
    }
    if (pmt->stream_num) {
	jarray = json_object_new_array();
	for (int n=0; n < pmt->stream_num; n++){
	    json_object_array_add(jarray, dvb_stream_json(pmt->stream[n]));
	}
	json_object_object_add(jobj, "streams", jarray);
    }
    return jobj;
}

json_object *dvb_transport_json(nit_transport *trans)
{
    json_object *jobj = json_object_new_object();
    json_object *jarray;

    json_object_object_add(jobj, "transport_stream_id",
			   json_object_new_int(trans->transport_stream_id));
    json_object_object_add(jobj, "original_network_id",
			   json_object_new_int(trans->original_network_id));

    if (trans->desc_num){
	jarray = json_object_new_array();
	uint32_t priv_id = 0;

	for (int n=0 ; n < trans->desc_num; n++){
 	    json_object_array_add(jarray,
				  dvb_descriptor_json(trans->descriptors[n],
						      &priv_id));
	}
	json_object_object_add(jobj, "descriptors", jarray);
    }
    return jobj;
}

json_object *dvb_nit_json(NIT *nit)
{
    json_object *jobj = json_object_new_object();
    json_object *jarray;

    json_object_object_add(jobj, "section data", dvb_section_json(nit->nit,0));
    json_object_object_add(jobj, "network_id",
			   json_object_new_int(nit->nit->id));
    json_object_object_add(jobj, "type",
			   json_object_new_string(
			       (nit->nit->table_id == 0x41) ?
			       "other":"actual"));

    if (nit->ndesc_num){
	jarray = json_object_new_array();
	uint32_t priv_id = 0;

	for (int n=0 ; n < nit->ndesc_num; n++){
 	    json_object_array_add(jarray,
				  dvb_descriptor_json(
				      nit->network_descriptors[n],
				      &priv_id));
	}
	json_object_object_add(jobj, "network_descriptors", jarray);
    }
    if (nit->trans_num){
	jarray = json_object_new_array();
	for (int n=0 ; n < nit->trans_num; n++){
 	    json_object_array_add(jarray,
				  dvb_transport_json(nit->transports[n]));
	}
	json_object_object_add(jobj, "transports", jarray);
    }
    return jobj;
}
