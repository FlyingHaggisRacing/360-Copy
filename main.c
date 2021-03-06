/* me7romtool [ namFirmware analysis tool for Bosch ME7.3H4 Ferrari firmware's]
   By 360trev. Needle lookup function borrowed from nyet (Thanks man!) from
   the ME7sum tool development (see github).
   
   See Readme for notes about current status and features.
 
   Permission is hereby granted, free of charge, to any person obtaining
   a copy of this software and associated documentation files (the
   "Software"), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
   OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
   BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
   AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
   OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
   IN THE SOFTWARE.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

#include "utils.h"
#include "needles.h"
#include "crc32.h"
#include "show_tables.h"
#include "table_spec.h"
#include "fixsums.h"
#include "find_dppx.h"
#include "mlhfm.h"

// this globals will be eliminated later (fixme)
char *rom_name=NULL;
char *hfm_name=NULL;
char *save_name=NULL;
int got_romfile=0;
int got_outfile=0;
int seedkey_patch=0;
int find_x_axis_maps=0;
int correct_checksums=0;
int force_write;
int valves=0;
int pedal=0;
//int find_dpp=1;
int find_mlhfm=0;
int full_debug=0;
int show_hex=0;
int show_adr=0;
int show_phy=1;
int show_help=0;
unsigned long dpp0_value, dpp1_value, dpp2_value, dpp3_value;

OPTS_ENTRY opts_table[] = {
//	  option      field to set        value to set  argument   req'd or not
	{ "-romfile", &got_romfile,       OPTION_SET,   &rom_name,  MANDATORY, "Try to identify map in the firmware. You *must* specify a romfile!\n"                             },
	{ "-outfile", &got_outfile,       OPTION_SET,   &save_name, MANDATORY, "Optional filename for saving romfiles after they have been modified (overrides default name)\n"   },
	{ "-force",   &force_write,       OPTION_SET,   0,          OPTIONAL,  "If a checksummed file needs saving overwrite it anyway even if it already exists.\n\n"            },

//	{ "-dppx",    &find_dpp,          OPTION_SET,   0,          OPTIONAL,  "Try to identify DPPx register settings to help with disassembly. (on as default)\n"               },
	{ "-KFAGK",   &valves,            OPTION_SET,   0,          OPTIONAL,  "Try to identify and show KFAGK exhaust valve opening table in the firmware.\n"                    },
	{ "-KFPED",   &pedal,             OPTION_SET,   0,          OPTIONAL,  "Try to identify and show KFPED/KFPEDR pedal torque request tables.\n\n"                           },
	
	{ "-rhfm",    &find_mlhfm,        HFM_READING,  &hfm_name,  MANDATORY, "Read and extract hfm from romfile, optional dump filename to override default write name.\n"      },
	{ "-whfm",    &find_mlhfm,        HFM_WRITING,  &hfm_name,  MANDATORY, "Write hfm into specified romfile. A Mandatory <hfm bin filename> must be specified.\n"            },
	{ "-ihfm",    &find_mlhfm,        HFM_IDENTIFY, &hfm_name,  OPTIONAL,  "Try to identify mlhfm table in specified romfile.\n"                                              },
	{ "-maps",    &find_x_axis_maps,  OPTION_SET,   0,          OPTIONAL,  "Try to identify map in the firmware (Experimental!).\n"                                           },
	{ "-seedkey", &seedkey_patch,     OPTION_SET,   0,          OPTIONAL,  "Try to identify seedkey function and patch login so any login password works.\n\n"                },

	{ "-fixsums", &correct_checksums, OPTION_SET,   0,          OPTIONAL,  "Try to correct checksums, if corrected it saves appending '_corrected.bin'.\n\n"                  },

	{ "-hex",     &show_hex,          OPTION_SET,   0,          OPTIONAL,  "Also show non formatted raw hex values in map table output.\n"                                    },
	{ "-adr",     &show_adr,          OPTION_SET,   0,          OPTIONAL,  "Also show non formatted raw hex values in map table output.\n"                                    },
	{ "-dbg",     &full_debug,        OPTION_SET,   0,          OPTIONAL,  "Show -phy (on as default), -hex and -adr in map table output.\n"                                  },
	{ "-nophy",   &show_phy,          OPTION_CLR,   0,          OPTIONAL,  "Override default behaviour and dont show formatted values in map table output.\n\n"               },
	
	{ "?",        &show_help,         OPTION_SET,   0,          OPTIONAL,  "Show this help.\n\n"                     },
};
	
int show_usage(char *argv[], int argc)
{
	printf("Usage: %s <options> ...\n\n",argv[0]);
	show_cli_usage(argc, argv, &opts_table, sizeof(opts_table)/sizeof(OPTS_ENTRY));
	return 0;
}

int main(int argc, char *argv[])
{
    int ok,required;
    int i,j, result;
	printf("Ferrari 360 ME7.3H4 Rom Tool. *BETA TEST* Last Built: %s %s v1.4\n",__DATE__,__TIME__);
	printf("by 360trev.  Needle lookup function borrowed from nyet (Thanks man!) from\nthe ME7sum tool development (see github). \n\n");

	/*
	 * parse and check which options provided by console
	 */	
    for (i = 0; i < argc; i++) 
	{
		result = parse_cli_options(argc, argv,i, &opts_table, sizeof(opts_table)/sizeof(OPTS_ENTRY));
		if(result == 1) { exit(0); }
	}

	/* if no arguments are specified show usage */
	if(argc < 2 || show_help == 1) {
		show_usage(argv, argc);
		return 0;
	}

	/* only proceed if we have been passed a valid romfile name */
	if(rom_name == 0) {
		printf("**No rom filename specified, e.g. -romfile rom.bin\n\n");
		return 0;		
	}

	/*
	 * sanity check any options for the given operational find_mlhfm 
	 */
	switch(find_mlhfm)
	{
		case HFM_READING:
		{
			if(hfm_name == NULL) 
			{	
				printf("You didnt specify a filename for HFM table so one will be automatically created!\n");
			} else {
				printf("HFM Output Filename: '%s'\n",hfm_name);
			}	
		} break;
		
		case HFM_WRITING:
		{
			if(hfm_name != NULL ) {
				printf("HFM Input Filename: '%s'\n",hfm_name);
			} else {
				printf("You didnt specify a <mandatory> filename for HFM table to load.\n");
				return 0;
			}
		} break;
		default:
		break;
	}
		
	/*
	 * now lets search rom for find_mlhfm selected..
	 */
    ok = search_rom(find_mlhfm, rom_name,hfm_name);
    if (ok) { 
		printf("File could not be found. Please check and try again.");  
	}

	return 0;
}

int search_rom(int find_mlhfm, char *filename_rom, char *filename_hfm)
{
	ImageHandle f;
	ImageHandle *fh = &f;
	int load_result;
//	int save_result;
//	int segment_offset;
	unsigned char *addr;
	unsigned char *offset_addr;
	unsigned short offset,entries;
	unsigned long dynamic_ROM_FILESIZE=0;
	uint32_t i;
	int hiword, loword;
	
	/* load file from storage */
	load_result = iload_file(fh, filename_rom, 0);
	if(load_result == 0) 
	{
		printf("Succeded loading file.\n\n");
		offset_addr = (unsigned char *)(fh->d.p);
		
		/* quick sanity check on firmware rom size (Ferrari 360 images must be 512kbytes/1024kbytes) */
		if(fh->len == ROM_FILESIZE*1 || fh->len == ROM_FILESIZE*2 ) 
		{		
			dynamic_ROM_FILESIZE = fh->len;	// either 512kbytes or 1024kbytes set depending on actual file size...

			if(dynamic_ROM_FILESIZE == ROM_FILESIZE*1 ) {
				printf("Loaded ROM: Tool in 512Kb Mode\n");
			} else if(dynamic_ROM_FILESIZE == ROM_FILESIZE*2 ) {
				printf("Loaded ROM: Tool in 1Mb Mode\n");
			}
			printf("\n");

//			show_hex_dump(offset_addr+0x10000, 0x200, 0x100000);

//-[ DPPx Search ] -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------			

			printf("-[ DPPx Setup Analysis ]-----------------------------------------------------------------\n\n");

			printf(">>> Scanning for Main ROM DPPx setup #1 [to extract dpp0, dpp1, dpp2, dpp3 from rom] ");
			addr = search( fh, (unsigned char *)&needle_dpp, (unsigned char *)&mask_dpp, needle_dpp_len, 0 );
			if(addr == NULL) {
				printf("\nmain rom dppX byte sequence #1 not found\nProbably not an ME7.x firmware file!\n");
				exit(0);	// force quit program

			} else {
				printf("\nmain rom dppX byte sequence #1 found at offset=0x%x.\n",(int)(addr-offset_addr) );
				// do the work of dppx extraction...
				dpp0_value = extract_dppx(addr,0);
				dpp1_value = extract_dppx(addr,1);
				dpp2_value = extract_dppx(addr,2);
				dpp3_value = extract_dppx(addr,3);
			}
	
//-[ Seedkey Version #1 ] -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------			
			/*
			 * search: *** Seed/Key Check Patch #1 ***
			 */
			 
			if(seedkey_patch == OPTION_SET) 
			{
				printf("\n-[ SeedKey Security Access ]-------------------------------------------------------------\n");

				printf("\n>>> Scanning for SecurityAccessBypass() Variant #1 Checking sub-routine [allow any login seed to pass] \n");
				addr = search( fh, (unsigned char *)&needle_5, (unsigned char *)&mask_5, needle_5_len, 0 );
				if(addr != NULL) {
					seedkey_patch = 2;		// since we discovered this, set this to 2 to skip unrequired secondary check
					printf("Found at offset=0x%x. ",(int)(addr-offset_addr) );
					// do the work of patching...
					printf("Applying patch so any login seed is successful... ");
					addr[0x5d] = 0x14; 
					printf("Patched!\n");
				}
				printf("\n");
			} 
			
//-[ Seedkey Version #2 ] -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------			
			/*
			 * search: *** Seed/Key Check Patch #2 ***
			 */

			if(seedkey_patch == OPTION_SET) 
			{
				printf("\n>>> Scanning for SecurityAccessBypass() Variant #2 Checking sub-routine [allow any login seed to pass] \n");
				addr = search( fh, (unsigned char *)&needle_6, (unsigned char *)&mask_6, needle_6_len, 0 );
				if(addr != NULL) {
					printf("Found at offset=0x%x. ",(int)(addr-offset_addr) );
					// do the work of patching...
					printf("Applying patch so any login seed is successful... ");
					addr[0x64] = 0x14; 	// very simple patch to always return TRUE...
					printf("Patched!\n");
				}
				printf("\n\n");
			}

//-[ KFAGK Table : Exhaust Valve Opening #1 ] ---------------------------------------------------------------------------------------------------------------------------------------------------------
		/*
		 * search: *** KFAGK Routine Variant #1 or Variant #2 ***
	 	 *
		 * KFAGK defines the characteristics map for exhaust flap changeover. This changes if the car has a loud or 
		 * quiet exhaust sound depending on the Throttle % and Engine RPM speed.
		 */
		if(valves == OPTION_SET) {
			printf("\n-[ Exhaust Valve KFAGK Table ]---------------------------------------------------------------------\n\n");
			{		
				printf(">>> Scanning for KFAGK Table #1 Checking sub-routine Variant #1 [manages exhaust valve/flap opening] \n");
				addr = search( fh, (unsigned char *)&KFAGK_needle, (unsigned char *)&KFAGK_mask, KFAGK_needle_len, 0 );
				if(addr == NULL) {
					printf("\n>>> Scanning for KFAGK Table #1 Checking sub-routine Variant #2 [manages exhaust valve/flap opening] \n");
					addr = search( fh, (unsigned char *)&KFAGK_needle2, (unsigned char *)&KFAGK_mask2, KFAGK_needle2_len, 0 );
				}
				if(addr != NULL) {
					printf("Found at offset=0x%x \n",(int)(addr-offset_addr) );
					// dump KFAGK table
					dump_table(addr, offset_addr, get16((unsigned char *)addr + 2), get16((unsigned char *)addr + 6), &KFAGK_table, 0);						
				}
				printf("\n\n");
			}
		}

		/*
		 * search: *** KFPEDR/KFPED Routine #1 ***
	 	 *
		 * KFPEDR/KFPED defines the characteristics map for Throttle Pedal torque requests.
		 */
		if(pedal == OPTION_SET) {
			printf("\n-[ Throttle Pedal KFPED/KFPEDR Table ]---------------------------------------------------------------------\n\n");
			{		
				printf(">>> Scanning for KFPED/KFPEDR Table #1 Checking sub-routine Variant #1 [manages throttle pedal torque requests] \n");
				addr = search( fh, (unsigned char *)&KFPED_needle, (unsigned char *)&KFPED_mask, KFPED_needle_len, 0 );
				if(addr != NULL) {
					printf("Found at offset=0x%x\n\n",(int)(addr-offset_addr));
					// dump KPEDR table
					dump_table(addr, offset_addr, get16((unsigned char *)addr + 14), dpp1_value-1, &KPEDR_table, 0);
					// dump KPED table (found table, rom, val, segment, table def)
					dump_table(addr, offset_addr, get16((unsigned char *)addr + 36), dpp1_value-1, &KPED_table, 0);
				}
				printf("\n\n");
			}
		}

//-[ Map Table Finder :) ] -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------			

/*  Things get quite interesting here on in...
 * 
 *  What I am about to show is how to identify 'generic' map tables directly from the rom function signatures ;)
 *  The idea is we directly find signatures for all 'known' maps, then find ALL maps and remove the known ones from the
 *  list. This yields a good 'catch all' ...
 * 
 *  The approach of masking all segment and relocation information out of the signatures means it works on any ME7x rom file 
 *  compiled for C167x cpu and works right across a huge number of rom variants.
 *
 *  This is just showing X-Axis tables (entire set of rom tables will come shortly and then we can easily match them too!)... 
 *  But ofcourse its quite simple to make this work for ALL the ROM resident tables. 
 * 
 *  This is a far better way than 'guessing' the maps knowing they reside (as some even commercial tools do) within a certain range 
 *  in the rom. This guarentee's your actually looking at real tables. The next step is to push the table start addresses into a hash 
 *  table to make it easy to de-duplicate them so you don't find calls to the lookups to the same tables (happens occasionally since 
 *  we are walking through the rom code and literaly picking up ALL of the accesses to the tables.
 * 
 *  Have fun ;)
 */
			if(find_x_axis_maps == OPTION_SET)
			{
				printf("-[ Generic X-Axis MAP Table Scanner! ]---------------------------------------------------------------------\n\n");
				printf(">>> Scanning for Map Tables #1 Checking sub-routine [map finder!] \n");
				
				int current_offset=0;
				int x,y1, i=0;
#if 1
				while(1)
				{
					// search for signature for X-Axis (1 row) table..
					addr = search( fh, (unsigned char *)&mapfinder_needle, (unsigned char *)&mapfinder_mask, mapfinder_needle_len, current_offset);

					// exit the searching loop when we reach end of rom region
					if(addr-offset_addr > dynamic_ROM_FILESIZE-mapfinder_needle_len) { break; }

					// if we find a match lets dump it!
					if(addr != NULL) {
						printf("\n[Map #%d] 1D X-Axis  : Map function found at: offset=0x%x ",(i++)+1, (int)(addr-offset_addr) );
						unsigned long val          = get16((unsigned char *)addr + 2);	// from rom routine extract value (offset in rom to table)
						unsigned long seg          = get16((unsigned char *)addr + 6);	// and segment (required to regenerate physical address from segment)
						unsigned long map_adr      = (unsigned long)(seg*SEGMENT_SIZE)+(long int)val;	// derive phyiscal address from offset and segment
						map_adr                   &= ~(ROM_1MB_MASK);					// convert physical address to a rom file offset we can easily work with.

						unsigned char *table_start = offset_addr+map_adr+1;			// 2 bytes to skip x and y bytes
						unsigned char x_axis       = get16(offset_addr+map_adr+0);		// get number of rows
//						unsigned char x_axis       = *(offset_addr+map_adr+1);		// get number of colums
						
//						printf("(seg:0x%x phy:0x%x val:0x%x), offset=0x%x x-axis=%d",seg, seg*SEGMENT_SIZE+val, val, (unsigned long)table_start-(int)offset_addr, x_axis);
						printf("phy:0x%x, file-offset=0x%x x-axis=%d",seg*SEGMENT_SIZE+val, (unsigned long)table_start-(int)offset_addr, x_axis);

						printf("\n\t");
						for(x=0;x<x_axis;x++) 
						{
							printf("%-2.2x ", (int)(*(table_start+x)) );	// show values directly out of the table
						}
					}
					printf("\n");
					
					// continue search from next location after this match...
					current_offset = (addr-offset_addr)+mapfinder_needle_len;

					//printf("\nCurrent Offset : %x\n",current_offset);
				}
#endif
				printf("\n\n");

				//
				// Multi-dimensional Type #1
				//
				{
					current_offset = 0;
					int new_offset=0;
					unsigned char *tmp_ptr;
					unsigned char *map_table_start;
					unsigned int map_table_adr;
					unsigned int map_table_x_axis_adr;
					unsigned int map_table_y_axis_adr;
					unsigned int map_table_x_num_adr;
					unsigned int x_num, y_num;
					unsigned int map_table_y_num_adr;
					unsigned long val, seg;
					
					while(1)
					{
						// search for signature for X/Y-Axis (multirow/column) table..
						current_offset = search_offset(offset_addr+new_offset, (fh->len)-new_offset, (unsigned char *)&mapfinder_xy2_needle, (unsigned char *)&mapfinder_xy2_mask, mapfinder_xy2_needle_len, 0);
						if(current_offset == 0) break;

						// if we find a match lets dump it!
						if(current_offset != NULL) {
							printf("\n------------------------------------------------------------------\n[Map #%d] Multi Axis Map Type #1 function found at: offset=0x%x \n",(i++)+1, (int)(current_offset+new_offset) );
							addr = offset_addr+current_offset+new_offset;
							dump_table(addr, offset_addr, get16((unsigned char *)addr + 30), get16((unsigned char *)addr + 26), &XXXX_table, 0);
							new_offset += current_offset+mapfinder_xy2_needle_len;
						}
						printf("\n");
					}
					printf("\n\n");
				}

				//
				// Single-dimensional Type #1
				//
				{
					current_offset = 0;
					int new_offset=0;
					unsigned int map_table_adr;
					unsigned int map_axis_adr;
					unsigned long val, seg;
					
					while(1)
					{
						// search for signature for X/Y-Axis (multirow/column) table..
						current_offset = search_offset(offset_addr+new_offset, (fh->len)-new_offset, (unsigned char *)&mapfinder_xy3_needle, (unsigned char *)&mapfinder_xy3_mask, mapfinder_xy3_needle_len, 0);
						if(current_offset == 0) break;

						// if we find a match lets dump it!
						if(current_offset != NULL) {
							printf("\n------------------------------------------------------------------\n[Map #%d] Multi Map Type #2 lookup function found @ offset: 0x%x \n",(i++)+1, (int)(current_offset+new_offset) );

							addr = offset_addr+current_offset+new_offset;
							unsigned char *pos=addr;
							unsigned char val;
							int i=0;

							// lets try to determine (experimental!!!) start of main subroutine this belongs too
							while(1)
							{
								i++;
								if(*(pos+0) == 0xDB)
								{
										if(*(pos+1) == 0x00) {
											i++;
											printf("\nFound estimated function start address: 0x%-8x", pos-offset_addr);
											break;
										}
								}
								if(pos <= offset_addr) { printf("not found\n"); break; } 
								pos--;
							}
							printf("\nBacktrack offset: 0x%x (%d bytes)\n\n",(int)(current_offset+new_offset+4)-i,i);
							
							val                   = get16((unsigned char *)addr + 14 - 10) ;	// from rom routine extract value (offset in rom to table)
							seg                   = get16((unsigned char *)addr + 18 - 10);	// and segment (required to regenerate physical address from segment)
							map_table_adr         = (unsigned long)(seg*SEGMENT_SIZE)+(long int)val;	// derive phyiscal address from offset and segment
							dump_table(addr, offset_addr, get16((unsigned char *)addr + 22 - 10), get16((unsigned char *)addr + 26 - 10), &XXXXB_table, map_table_adr);

							new_offset += current_offset+mapfinder_xy2_needle_len;
						}
						printf("\n");
					}
					printf("\n\n");
				}
				
				
			}

			// mlhfm support
			check_mlhfm(fh, addr, filename_rom, filename_hfm, dynamic_ROM_FILESIZE, offset_addr);

			// do correction
			fix_checksums(fh, addr, filename_rom, dynamic_ROM_FILESIZE, offset_addr);

		} else {
			printf("File size isn't a supported firmware size. Only 512kbyte and 1Mb images supported. ");
		}

			
/*------------------------------------------------------------------------------------------------------------------*/
	} else {
		printf("\nFailed to load, result = %d\n", load_result);
	}
	/* free file if allocated */
	load_result = ifree_file(fh);
	printf("---\n");
	return 0;
}
