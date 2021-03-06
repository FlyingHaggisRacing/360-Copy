/*
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
#include "show_tables.h"
#include "utils.h"

// defined in main.c (TODO: this needs cleaning up!)
extern int full_debug;
extern int show_hex;
extern int show_adr;
extern int show_phy;

//get_nwidth
unsigned long get_nwidth(unsigned char *s, int nwidth)
{
	unsigned long val=0;
	
	switch(nwidth)
	{
		case 1:		// 8-bit support
			val = (unsigned long)*(s);
			break;
		case 2:		// 16-bit support
			val = (unsigned long)get16(s);
			break;
		case 4:		// 32-bit support
			val = (unsigned long)get32(s);
			break;
		default:
//			printf("FORMAT NOT SUPPORTED! FIXME\n");
			break;
	}
	return val;
}

void show_entry_def(ENTRY_DEF *entry, int nwidth)
{
	char *conv_formula[32];

	if(nwidth == 0) return;
	
	printf("\n    %s:\n",entry->field_name);  
	printf("      Unit:                    %s\n",entry->desc);
	printf("      Conversion name:         %s\n",entry->conv_name);
	snprintf(conv_formula, 32, "f(phys) = 0.0 + %3.2f * phys", strtod(entry->conv,NULL) );
	printf("      Conversion formula:      %s\n",conv_formula);
	printf("      Data type:               ");
	switch(nwidth)
	{
		case 1:	  printf("UBYTE"); break;
		case 2:	  printf("UWORD"); break;
		case 4:	  printf("ULONG"); break;
		default:  printf("%d BYTES", nwidth);
	}
}

int dump_table(unsigned char *adr, unsigned char *offset_addr, unsigned long val, unsigned long seg, TABLE_DEF *td, unsigned long cell_table_override_adr)
{
	int x,y;
	int seg_start             = 0x800000;
	unsigned long rom_adr      = (unsigned long)(seg*SEGMENT_SIZE)+(long int)val;	// derive phyiscal address from offset and segment
	unsigned long map_table_adr= rom_adr;
	map_table_adr             &= ~(ROM_1MB_MASK);					// convert physical address to a rom file offset we can easily work with.

	unsigned char *table_start =   offset_addr+map_table_adr;			// 2 bytes to skip x and y bytes
	unsigned long  table_start2 =  cell_table_override_adr;			// 2 bytes to skip x and y bytes
	table_start2               &= ~(ROM_1MB_MASK);					// convert physical address to a rom file offset we can easily work with.
	table_start2              += offset_addr;
	
	double double_cell_value;
	int    i,x_pos,y_pos;
	int    entry;
	int    integer_cell_value;
	int    table_data_offset;
	double y_axis_hdr_value_raw;
	double y_axis_hdr_value_fmt;
	unsigned char  *x_num_data_start;
	unsigned char  *y_num_data_start;
	unsigned char  *x_axis_header_data_start;
	unsigned char  *y_axis_header_data_start;
	unsigned char  *cell_data_start;
	unsigned int x_num, y_num, line=0;
//	unsigned int bytes_per_x_row;
//	unsigned int bytes_per_y_row;
//	char *conv_formula[32];
	unsigned char *cell_adr;

	// get the cell data
		if(cell_table_override_adr == 0)
		{
			table_data_offset  = 0;
			x_num_data_start          = table_start + table_data_offset;
			table_data_offset        += (td->x_num_nwidth);				// x num
			x_num 					  = get_nwidth(x_num_data_start, td->x_num_nwidth);		// get number of colums   **FIXED**

			y_num_data_start          = table_start + table_data_offset;
			table_data_offset        += (td->y_num_nwidth);				// y num
			y_num                     = get_nwidth(y_num_data_start, td->y_num_nwidth);		// get number of rows	  **FIXED**

			x_axis_header_data_start  = table_start + table_data_offset;
			table_data_offset        += (x_num*td->x_axis_nwidth);		//x axis header data

			y_axis_header_data_start  = table_start + table_data_offset;
			table_data_offset        += (y_num*td->y_axis_nwidth);		// y axis header data

			cell_data_start           = table_start + table_data_offset;
			printf("Defaulting to implide cell_data start address at %p\n", cell_data_start);
		} else {
			table_data_offset  = 0;
			x_num_data_start          = table_start + table_data_offset;
			table_data_offset        += (td->x_num_nwidth);				// x num
//			printf("X_NUM  start address: %p\n", x_num_data_start - offset_addr);
			x_num 					  = get_nwidth(x_num_data_start, td->x_num_nwidth);		// get number of colums   **FIXED**

			x_axis_header_data_start  = table_start + table_data_offset;
			table_data_offset        += (x_num*td->x_axis_nwidth);		//x axis header data
//			printf("X_AXIS start address: %p\n", x_axis_header_data_start - offset_addr);

			y_num_data_start          = table_start + table_data_offset;
			table_data_offset        += (td->y_num_nwidth);				// y num
//			printf("Y_NUM  start address: %p\n", y_num_data_start - offset_addr);
			y_num                     = get_nwidth(y_num_data_start, td->y_num_nwidth);		// get number of rows	  **FIXED**

			y_axis_header_data_start  = table_start + table_data_offset;
			table_data_offset        += (y_num*td->y_axis_nwidth);		// y axis header data
//			printf("Y_AXIS start address: %p\n", y_axis_header_data_start - offset_addr);

			cell_data_start           = table_start2 ;
			printf("Overriding cell_data start address to %p\n", cell_data_start);
			
		}

//		bytes_per_x_row           = (x_num*td->cell_nwidth);
//		bytes_per_y_row           = (y_num*td->cell_nwidth);

		// if we turn on full debug support, turn on flags to show everything		
		if(full_debug == 1) {
			show_phy = 1;
			show_adr = 1;
			show_hex = 1;
		}
//					printf("    xnum=%d (nwidth=%d), ynum=%d (nwidth=%d)\n", x_num, td->x_num_nwidth, y_num, td->y_num_nwidth);  

					printf("\n%s\n",td->table_name);
					printf("    Long identifier:           %s\n", td->table_desc);
					printf("    Display identifier:        %s\n", " ");  

//					unsigned long tmp = (unsigned long)cell_data_start;
//					tmp = tmp - (unsigned long)offset_addr;
//					printf("    Cell Address:              0x%x\n", (char *)tmp );	

					printf("    Address:                   0x%x\n", (char *)rom_adr  );	
					printf("    Value:\n\n");  

					printf(" No.           | ");
					for(i=0;i<x_num;i++) {
						printf("    %4d ", i);
					}

					if(show_phy == 1)
					{
						printf("\n            PHY| ");
						for(i=0;i<x_num;i++) {
							entry = get_nwidth(x_axis_header_data_start+(i*td->x_axis_nwidth), td->x_axis_nwidth);	//**FIXED**
							printf(td->x_axis.fmt_PHY, entry/strtod(td->x_axis.conv,NULL) );		// convert stored table value to correct viewing format
						}
						line++;
					}

					if(show_hex == 1)
					{
						printf("\n            HEX| ");
						for(i=0;i<x_num;i++) {
							entry = get_nwidth(x_axis_header_data_start+(i*td->x_axis_nwidth), td->x_axis_nwidth);	//**FIXED**
							printf(td->cell.fmt_HEX, entry );						// show raw hex
						}
						line++;
					}

					if(show_adr == 1)
					{
						printf("\n            ADR| ");
						for(i=0;i<x_num;i++) {
							printf("0x%X ", (x_axis_header_data_start+(i*td->x_axis_nwidth ) - offset_addr + seg_start ));
						}
						line++;
					}
					
					printf("\n");
					printf(" --------------+");
					for(i=0;i<x_num;i++) {
						printf("---------");
					}

					for(y_pos=0;y_pos <y_num;y_pos++) 
					{
#if 1
						// get y_axis header data
						unsigned char *adr = y_axis_header_data_start + (y_pos*td->y_axis_nwidth);
						y_axis_hdr_value_raw = get_nwidth( adr, td->y_axis_nwidth );		//**FIXED**
						y_axis_hdr_value_fmt = y_axis_hdr_value_raw/(strtod(td->y_axis.conv,NULL));	

						if(show_phy==1)
						{
							printf("\n ");
							printf(td->y_axis.fmt_PHY, (double)y_axis_hdr_value_fmt );
							printf("    PHY| ");
							for(x_pos=0;x_pos<x_num;x_pos++) 
							{
								// get cell data
								entry = get_nwidth( (cell_data_start + (x_pos)*(y_num*td->cell_nwidth) + y_pos*td->cell_nwidth ), td->cell_nwidth ); 	//**FIXED**
								// show floating point formatted value
								double_cell_value = (double)entry;
								double_cell_value = (double)double_cell_value / (strtod(td->cell.conv,NULL));
								printf(td->cell.fmt_PHY,   (double)double_cell_value );	// show values directly out of the table
							}
						}

						if(show_hex==1)
						{
							// get y_axis formatting and display it
							y_axis_hdr_value_fmt = y_axis_hdr_value_raw/(strtod(td->y_axis.conv,NULL));	
							printf("\n  ");
							printf("%-#8.4x ", get_nwidth( (int)adr, td->cell_nwidth ) );						//**FIXED**
							printf(" HEX| ");
							for(x_pos=0;x_pos<x_num;x_pos++) 
							{
								cell_adr = (cell_data_start + (x_pos)*(y_num*td->cell_nwidth) + y_pos*td->cell_nwidth );	// get cell address
								entry    = get_nwidth( cell_adr, td->cell_nwidth );										//**FIXED**// get cell data
								printf(td->cell.fmt_HEX,(int)entry);						// show values directly out of the table
							}
						}

						if(show_adr==1)
						{
							// get y_axis formatting and display it
							y_axis_hdr_value_fmt = y_axis_hdr_value_raw/(strtod(td->y_axis.conv,NULL));	
							printf("\n  ");
							printf("%-#9.5x", (int)(adr - (unsigned char *)(offset_addr))+seg_start );
							printf(" ADR| ");
							for(x_pos=0;x_pos<x_num;x_pos++) 
							{	
								cell_adr = (cell_data_start + (x_pos)*(y_num*td->cell_nwidth) + y_pos*td->cell_nwidth );	// get cell address
								entry    = get_nwidth( cell_adr, td->cell_nwidth );									//**FIXED**	// get cell data
								printf("0x%x ", cell_adr - offset_addr + seg_start );			// show address
							}
						} 
						
						if(line > 1)
							printf("\n");					
#endif						
					}					

		printf("\n\n");
		show_entry_def(&td->cell,   td->cell_nwidth);
		show_entry_def(&td->x_axis, td->x_axis_nwidth);
		show_entry_def(&td->y_axis, td->y_axis_nwidth);
		printf("\n\n");
		return 0;
}
