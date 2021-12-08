/*
Copyright (C) 2021  Marcus Metzler

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include "dvb.h"
#include "dvb_service.h"

#define BUFFSIZE (1024*188)

void print_help()
{
    dvb_print_tuning_options();
    fprintf(stderr,
	    "\n OTHERS:\n"
	    " -N           : get NIT\n"
	    " -S           : get SDT\n"
	    " -P           : get PAT and PMTs\n"
	    " -F           : do full NIT scan\n"
	    " -o filename  : output filename\n"
	    " -h           : this help message\n\n"
	);
}

int parse_args(int argc, char **argv, dvb_devices *dev,
	       dvb_fe *fe, dvb_lnb *lnb, char *fname)
{
    int out = 0;
    if (dvb_parse_args(argc, argv, dev, fe, lnb)< 0) return -1;

    optind = 1;

    while (1) {
	int option_index = 0;
	int c;
	static struct option long_options[] = {
	    {"fileout", required_argument , 0, 'o'},
	    {"nit", required_argument , 0, 'N'},
	    {"sdt", required_argument , 0, 'S'},
	    {"pat", required_argument , 0, 'P'},
	    {"full_scan", required_argument , 0, 'F'},
	    {"stdout", no_argument , 0, 'O'},
	    {"help", no_argument , 0, 'h'},
	    {0, 0, 0, 0}
	};
	
	c = getopt_long(argc, argv, "ho:ONSPF", long_options, &option_index);
	if (c==-1)
	    break;
	
	switch (c) {
	case 'o':
	    fname = strdup(optarg);
	    out = 1;
	    break;

	case 'N':
	    out = 2;
	    break;

	case 'S':
	    out = 3;
	    break;

	case 'P':
	    out = 4;
	    break;

	case 'F':
	    out = 5;
	    break;

	case 'O':
	    out = 1;
	    break;
	    
	case 'h':
	    print_help();
	    return -1;
	    break;
	default:
	    break;
	}
    }

    return out;
}

#define MAXTRY 10
int tune(dvb_devices *dev, dvb_fe *fe, dvb_lnb *lnb)
{
    int re = 0;
    int t= 0;
    int lock = 0;
    switch (fe->delsys){
    case SYS_DVBC_ANNEX_A:
	fprintf(stderr,
		"Tuning freq: %d kHz sr: %d delsys: DVB-C  ",
		fe->freq, fe->sr);
	if ((re=dvb_tune_c( dev, fe)) < 0) return 0;
	break;
	
    case SYS_DVBS:
    case SYS_DVBS2:	
	fprintf(stderr,
		"Tuning freq: %d kHz pol: %s sr: %d delsys: %s "
		"lnb_type: %d input: %d  ",
		fe->freq, fe->pol ? "h":"v", fe->sr,
		fe->delsys == SYS_DVBS ? "DVB-S" : "DVB-S2",
		lnb->type, fe->input);
	if ((re=dvb_tune_sat( dev, fe, lnb)) < 0) return 0;
	break;


    case SYS_DVBT:
    case SYS_DVBT2:
    case SYS_DVBC_ANNEX_B:
    case SYS_ISDBC:
    case SYS_ISDBT:
    case SYS_ISDBS:
    case SYS_UNDEFINED:
    default:
	fprintf(stderr,"Delivery System not yet implemented\n");
	return 0;
	break;
    }

    while (!lock && t < MAXTRY ){
	t++;
	fprintf(stderr,".");
	lock = read_status(dev->fd_fe);
	sleep(1);
    }
    if (lock == 2) {
	fprintf(stderr," tuning timed out\n");
    } else {
	fprintf(stderr,"%slock\n",lock ? " ": " no ");
    }
    return lock;
}

descriptor *find_descriptor(descriptor **desc, int length, uint8_t tag)
{
    descriptor *d = NULL;;

    for (int i=0;i< length; i++){
	if (desc[i]->tag == tag){
	    d = desc[i];
	    return d;
	}
    }

    return NULL;
}

nit_transport *find_nit_transport(NIT **nits, uint16_t tsid)
{
    int n = nits[0]->nit->last_section_number+1;
    nit_transport *trans = NULL;
    fprintf(stderr,"searching transport tsid 0x%04x\n",tsid);
    for(int i = 0; i < n; i++){
	for (int j = 0; j < nits[i]->trans_num; j++){
	    trans = nits[i]->transports[j];
	    if (trans->transport_stream_id == tsid){
		fprintf(stderr,"Found transport with complete NIT/BAT\n");
		return trans;
	    }
	}
    }
    return NULL;
}

int get_all_services(transport *trans, dvb_devices *dev)
{
    int sdt_snum=0;
    int pat_snum=0;
    int snum = 0;

    if (!trans->pat) return -1;
    if (trans->sdt){
	for (int n=0; n < trans->nsdt; n++)
	    sdt_snum += trans->sdt[n]->service_num;
    }
    for (int n=0; n < trans->npat; n++)
	    pat_snum += trans->pat[n]->nprog;

    snum = ((pat_snum >= sdt_snum) ? pat_snum : sdt_snum);
    trans->nserv = snum;
    trans->serv = (service *) malloc(snum*sizeof(service));
    for (int j=0; j < snum; j++) {
	trans->serv[j].sdt_service = NULL;
	trans->serv[j].id  = 0;
	trans->serv[j].pmt  = NULL;
	trans->serv[j].sat  = trans->sat;
	trans->serv[j].trans  = trans;
    }
    int i=0;
    if (trans->sdt){
	for (int n=0; n < trans->nsdt; n++){
	    for (int j=0; j < trans->sdt[n]->service_num; j++){
		trans->serv[i].sdt_service = trans->sdt[n]->services[j];
		trans->serv[i].id  = trans->sdt[n]->services[j]->service_id;
		i++;
	    }
	}
    } 
    
    for (int n=0; n < trans->npat; n++){
	for (int i=0; i < trans->pat[n]->nprog; i++){
	    int npmt = 0;
	    uint16_t pid = trans->pat[n]->pid[i];
	    if (!trans->pat[n]->program_number[i]) continue;
	    PMT  **pmt = get_all_pmts(dev, pid);
	    if (pmt){
		npmt = pmt[0]->pmt->last_section_number+1;
		for (int k=0; k < npmt; k++){
		    int j=0;
		    while (trans->pat[n]->program_number[i] !=
			trans->serv[j].id && j < i){
			j++;
		    }
		    trans->serv[j].pmt = pmt;
		    if (j==i){ // in case there is no SDT entry
			trans->serv[j].id = trans->pat[n]->program_number[i];
			i++;
		    }
		}
	    }
	}
    }

    return snum;
}

satellite *full_nit_search(dvb_devices *dev, dvb_fe *fe, dvb_lnb *lnb)
{
    NIT **nits = NULL;
    nit_transport *trans;
    descriptor *desc = NULL;
    int n = 0;
    uint16_t tsid = 0;
    satellite *sat;
    
    fprintf(stderr,"Full NIT search\n");

    nits = get_all_nits(dev, 0x40);
    n = nits[0]->nit->last_section_number+1;

    for (int i=0; i<n; i++){
	for (int j=0; j < nits[i]->ndesc_num; j++){
	    desc = nits[i]->network_descriptors[j];
	    if (desc && desc->tag == 0X4a){
		if (desc->data[6] == 0x04){
		    tsid = (desc->data[0] << 8) | desc->data[1];
		    break;
		}
	    }
	}
    }
    if (tsid){
	uint32_t freq = fe->freq;
	if ((trans = find_nit_transport(nits,tsid))){
	    if (set_frontend_with_transport(fe, trans)) {
		fprintf(stderr,"Could not set frontend\n");
		exit(1);
	    }
	    if (freq != fe->freq){
		int lock = tune(dev, fe, lnb);
		if (lock == 1){ 
		    for (int i=0; i < n; i++){
			dvb_delete_nit(nits[i]);
		    }
		    nits = get_all_nits(dev, 0x40);
		    n = nits[0]->nit->last_section_number+1;
		}
	    }
	}
    }
    if(!(sat  = (satellite *) malloc(sizeof(satellite)))){
	fprintf(stderr,"Not enough memory for satellite\n");
	return NULL;
    }

    sat->nit = nits;
    sat->nnit = nits[0]->nit->last_section_number+1;
    dvb_copy_dev(&sat->dev, dev);
    dvb_copy_lnb(&sat->lnb, lnb);
    sat->ntrans = 0;
    for  (int i =0; i < sat->nnit; i++){
	sat->ntrans += sat->nit[i]->trans_num;
    }
    sat->trans = (transport *) malloc(sat->ntrans*sizeof(transport));
    int k= 0;
    for  (int i =0; i < sat->nnit; i++){
	for (int j=0; j < sat->nit[i]->trans_num; j++){
	    if (set_frontend_with_transport(fe,sat->nit[i]->transports[j])){
		fprintf(stderr,"Could not set frontend\n");
		exit(1);
	    }
	    dvb_copy_fe(&sat->trans[k].fe, fe);
	    sat->trans[k].nit_transport = sat->nit[i]->transports[j];
	    k++;
	}
    }
    for (k = 0; k < sat->ntrans; k++){
	transport *trans = &sat->trans[k];
	trans->sat = sat;
	int lock = tune(dev, &trans->fe, lnb);
	trans->lock = lock;
	if (lock == 1){ 
	    fprintf(stderr,"  getting SDT\n");
	    trans->sdt = get_all_sdts(dev);
	    trans->nsdt = trans->sdt[0]->sdt->last_section_number+1;
	    fprintf(stderr,"  getting PAT\n");
	    trans->pat = get_all_pats(dev);
	    trans->npat = trans->pat[0]->pat->last_section_number+1;
	    fprintf(stderr,"  getting PMTs\n");
	    trans->nserv = get_all_services(trans, dev);
	}
    }
    return sat;
}

void search_nit(dvb_devices *dev, uint8_t table_id)
{
    int fdmx;
    int re = 0;
    NIT **nits = NULL;
    int nnit = 0;
    
    fprintf(stderr,"Searching NIT\n");
    nits = get_all_nits(dev, table_id);
    nnit = nits[0]->nit->last_section_number+1;
    for (int n=0; n < nnit; n++)
	dvb_print_nit(fileno(stdout), nits[n]);
}

void search_sdt(dvb_devices *dev)
{
    int fdmx;
    int re = 0;
    SDT **sdts = NULL;
    int n = 0;
    
    fprintf(stderr,"Searching SDT\n");
    sdts = get_all_sdts(dev);
    n = sdts[0]->sdt->last_section_number+1;
    for (int i=0; i < n; i++)
	dvb_print_sdt(fileno(stdout), sdts[i]);
}

void search_pat(dvb_devices *dev)
{
    int fdmx;
    uint8_t sec_buf[4096];
    int re = 0;
    PAT **pats = NULL;
    int npat = 0;

    fprintf(stderr,"Searching PAT\n");
    pats = get_all_pats(dev);
    if (pats){
	npat = pats[0]->pat->last_section_number+1;
	for (int i=0; i < npat; i++){
	    dvb_print_pat(fileno(stdout), pats[i]);
	}

	for (int n=0; n < npat; n++){
	    for (int i=0; i < pats[n]->nprog; i++){
		int npmt = 0;
		uint16_t pid = pats[n]->pid[i];
		if (!pats[n]->program_number[i]) continue;
		fprintf(stderr,"Searching PMT 0x%02x for program 0x%04x\n",
			pid,pats[n]->program_number[i]);
		PMT  **pmt = get_all_pmts(dev, pid);
		if (pmt){
		    npmt = pmt[0]->pmt->last_section_number+1;
		    for (int i=0; i < npmt; i++){
			dvb_print_pmt(fileno(stdout), pmt[i]);
		    }
		}
	    }
	}
    }
}

int main(int argc, char **argv){
    dvb_devices dev;
    dvb_lnb lnb;
    dvb_fe fe;
    int lock = 0;
    int t=0;
    int out = 0;
    int fd = 0;
    char *filename = NULL;
    uint8_t sec_buf[4096];
    int re=0;
    
    dvb_init(&dev, &fe, &lnb);
    if ((out=parse_args(argc, argv, &dev, &fe, &lnb, filename)) < 0)
	exit(2);
    dvb_open(&dev, &fe, &lnb);
    if ((lock = tune(&dev, &fe, &lnb)) != 1) exit(lock);

    if (out && lock){
	switch (out) {
	    
	case 1:
	{
	    uint8_t *buf=(uint8_t *)malloc(BUFFSIZE);
	
	    if (filename){
		fprintf(stderr,"writing to %s\n", filename);
		fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC,
			  00644);
	    } else {
		fprintf(stderr,"writing to stdout\n");
		fd = fileno(stdout); 
	    }
	    while(lock){
		int re = read(dev.fd_dvr,buf,BUFFSIZE);
		re = write(fd,buf,re);
	    }
	    break;
	}
	case 2:
	    search_nit(&dev,0x40);
	    search_nit(&dev,0x41);
	    break;

	case 3:
	    search_sdt(&dev);
	    break;

	case 4:
	    search_pat(&dev);
	    break;

	case 5:
	    full_nit_search(&dev, &fe, &lnb);
	default:
	    break;
	}
    }
}



