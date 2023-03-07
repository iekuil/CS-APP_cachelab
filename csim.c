#include "cachelab.h"
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

typedef long long  addr;

typedef struct cache_line{
	addr	status_bit;
	addr	tag_bit;
} cl ;

typedef struct simulate_result{
	int	hit_counts;
	int	miss_counts;
	int	evict_counts;
} result;

typedef struct bit_masks{
	addr	sind_mask;
	addr	tag_mask;
	addr	vbit_mask;
} mask;

addr addr_to_setid( addr a );
addr addr_to_tag( addr a );
addr is_cl_valid( addr status );



int check_hit( addr dest_addr );
int check_spare_line( addr dest_addr );
int evict_a_line( addr dest_addr );
int load_a_line( addr dest_addr );

int init_cache();
int free_cache();
int init_masks();


int parse_text_line( char *buffer );
int parse_text();


int opt_v = 0;		// verbose flag
int opt_s = 0;		// s bits for set id
int opt_S = 0;		// S sets
int opt_E = 0;		// E lines each set
int opt_b = 0;		// b bits for block offset
char *opt_t;		// path of trace file
mask m;			// mask
cl *pcache;		// pointer to the cache
result res;

int main(int argc,char *argv[])
{
	
	int opt = 0;
	const char *optstring = "vs:E:b:t:";

	while((opt = getopt(argc,argv,optstring) )!= -1)
	{
		switch(opt){
				case 'v':opt_v = 1;break;
				case 's':opt_s = atoi(optarg);break;
				case 'E':opt_E = atoi(optarg);break;
				case 'b':opt_b = atoi(optarg);break;
				case 't':opt_t = optarg;
		}
	}
	opt_S = 1 << opt_s;

	// printf("v=%d,s=%d,E=%d,b=%d,t=%s\n",opt_v,opt_s,opt_E,opt_b,opt_t);
	//addr sind_mask = 0;
	//addr tag_mask = 0;
	//addr vbit_mask = 0;
	m.sind_mask = 0;
	m.tag_mask = 0;
	m.vbit_mask = 0;

	init_masks();
	printf("sind_mask=%llx,tag_mask=%llx\n",m.sind_mask,m.tag_mask);

	init_cache();

	parse_text();	

	free_cache();	
	printSummary(res.hit_counts, res.miss_counts, res.evict_counts);
	return 0;
}

int init_cache()
{
	pcache = malloc(opt_S * opt_E * sizeof(cl));
	memset( pcache , 0 , opt_S * opt_E * sizeof(cl));
	return 0;
}
int free_cache()
{
	free(pcache);
	return 0;
}


int init_masks()
{
	int i = 0;
	addr tmp = 1;
	for ( i = 0; i < opt_s; i++)
	{
		m.sind_mask = (m.sind_mask << 1) | tmp ;
	}
	for ( i = 0; i < opt_b; i++)
	{
		m.sind_mask = m.sind_mask << 1;
	}
	for ( i = 0; i < 64 - opt_s - opt_b; i++)
	{
		m.tag_mask = (m.tag_mask << 1) | tmp;
	}
	for ( i = 0; i < opt_s + opt_b;i++)
	{
		m.tag_mask = m.tag_mask << 1;
	}
	m.vbit_mask = ( 1UL << 63 );
	return 0;
}


int parse_text_line( char *buffer )
{
	char v_str[128];
	strcpy(v_str,buffer);
	//if ( opt_v )
	//	printf("%s",buffer);

	if (buffer[0] == 'I')
		return 0;
	char type = buffer[1];
	char *addr_begin = &buffer[3];
	int tmp_ind = 0;
	while(buffer[tmp_ind] != ',')
		tmp_ind ++ ;
	buffer[tmp_ind] = '\0';
	char *str;		//useless
	//addr dest_addr = atoll(addr_begin);
	addr dest_addr = 0;
	dest_addr = strtol(addr_begin,&str,16);

	
	if ( check_hit( dest_addr ) )
	{
		// hit
		if ( opt_v )
		{
			strcat(v_str,"  hit");
			printf("%s\n",v_str);
		}
		switch(type)
		{
			case 'M':res.hit_counts += 2;break;
			default:res.hit_counts += 1;

		}
		return 0;		
	}
	// miss
	//printf("  miss");
	strcat(v_str,"  miss");
	res.miss_counts += 1;
	
	if ( !check_spare_line( dest_addr )  )
	{
		// evict
		evict_a_line( dest_addr );
		strcat(v_str,"  evict");
		res.evict_counts += 1;
	}
	load_a_line(dest_addr);
	if (type == 'M')
	{
		strcat(v_str,"  hit");
		res.hit_counts += 1;
	}
	if (opt_v )
		printf("%s\n",v_str);
	return 0;	
	

}


int parse_text()
{
	FILE *pf;
	char buffer[128];
	
	if ( (pf = fopen( opt_t ,"r") ) == NULL )
	{
		printf("failed to open the file");
		exit(EXIT_FAILURE);
	}
	
	//while(!feof(pf))
	//{
	//	fgets(buffer,128,pf);
	//	buffer[strlen(buffer)-1] = '\0';
	//	parse_text_line( buffer );

	//}

	while(fgets(buffer,128,pf)!=NULL)
	{
		buffer[ strlen(buffer) -1] = '\0';
		parse_text_line ( buffer );
	}
	fclose( pf );
	return 0;
}



addr addr_to_setid( addr a )
{
	addr tmp_res = a & m.sind_mask ;
	tmp_res = tmp_res >> opt_b;
	return tmp_res;	
}


addr addr_to_tag( addr a )
{
	addr tmp_res = a & m.tag_mask;
	return tmp_res;
}

addr is_cl_valid( addr status )
{
	return status & m.vbit_mask ;
}

int check_hit( addr dest_addr )
{
	addr dest_sid = addr_to_setid( dest_addr );
	addr dest_tag = addr_to_tag( dest_addr );
	cl *set = pcache + dest_sid * opt_E;
	
	int i;
	int flag = 0;
	for ( i = 0; i < opt_E; i++ )
	{
		if ( set[i].tag_bit == dest_tag && is_cl_valid(set[i].status_bit) )
		{
			flag = 1;
			break;
		}
	}
	if ( flag == 1 )
	{
		int j;
		for ( j = 0; j < opt_E ; j++ )
		{
			if ( i!=j)
			{
				set[j].status_bit = ((set[j].status_bit & ~m.vbit_mask) + 1 ) | ( set[j].status_bit & m.vbit_mask ) ;	
			}
			else
			{
				set[j].status_bit = m.vbit_mask;
			}
		}
	}
	return flag;
}


int check_spare_line( addr dest_addr )
{
	addr dest_sid = addr_to_setid( dest_addr );
	cl *set = pcache + dest_sid * opt_E;
	
	int i;
	int flag = 0;
	for ( i = 0; i < opt_E ; i ++ )
	{
		if ( !is_cl_valid( set[i].status_bit ) )
		{
			flag = 1;
			break;
		}	
	}
	return flag;
}




int evict_a_line( addr dest_addr )
{

	addr dest_sid = addr_to_setid( dest_addr );
	cl *set = pcache + dest_sid * opt_E;

	int i = 0;
	int times_max = 0;
	int index = 0;
	int current_times = 0;

	for ( i = 0; i < opt_E ; i++ )
	{
		current_times = set[i].status_bit & (~m.vbit_mask) ;
		if ( current_times >= times_max )
		{
			index = i;
			times_max = current_times ;
		}	
	}
	set[index].status_bit = 0;
	return 0;
}


int load_a_line( addr dest_addr )
{
	addr dest_sid = addr_to_setid( dest_addr );
	cl *set = pcache + dest_sid * opt_E;
	addr dest_tag = addr_to_tag( dest_addr );

	int i;
	int flag = 0;
	for ( i = 0; i < opt_E; i++)
	{
		if ( !is_cl_valid( set[i].status_bit ) && flag == 0 )
		{
			set[i].tag_bit = dest_tag;
			set[i].status_bit = m.vbit_mask;
			flag = 1;
		}
		else
		{
			set[i].status_bit = ( (set[i].status_bit & (~m.vbit_mask)) + 1) | ( set[i].status_bit & m.vbit_mask );
		}
	}
	return 0;
}
